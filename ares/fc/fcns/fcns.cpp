#include <fc/fc.hpp>

namespace ares::Famicom {

FCNS fcns;
FCNSCard fcnsCard;
#include "socket.cpp"
#include "link.cpp"
#include "msm6827.cpp"
#include "uart.cpp"
#include "cpu2.cpp"
#include "serialization.cpp"

auto FCNS::Debugger::load(Node::Object parent) -> void {
  tracer.network = parent->append<Node::Debugger::Tracer::Notification>("Network", "FCNS");
}

auto FCNS::Debugger::unload(Node::Object parent) -> void {
  if(tracer.network) parent->remove(tracer.network);
  tracer.network = {};
}

auto FCNS::Debugger::network(string message) -> void {
  if(tracer.network && tracer.network->enabled()) tracer.network->notify(message);
}

auto FCNS::load() -> void {
  node = cartridge.node;
  if(node) debugger.load(node);
  link.open();
  cpu2.ram.allocate(0x2000, 0xff);
  characterRAM[0].allocate(0x2000, 0x00);
  characterRAM[1].allocate(0x2000, 0x00);
  wram.allocate(0x2000, 0xff);
}

auto FCNS::unload() -> void {
  link.socket.close();
  if(node) debugger.unload(node);
  node = {};
  if(fcnsCard.port) fcnsCard.unload();
  cpu2.unload();
  cpu2.rom.reset();
  cpu2.ram.reset();
  kanjiROM.reset();
  characterRAM[0].reset();
  characterRAM[1].reset();
  wram.reset();
  present = 0;
}

//the M2 cycle counter, clocked once per CPU cycle from FCNS::main(). Mirrors FDSTimer::clock().
auto FCNS::Timer::clock() -> void {
  if(!enable) return;
  if(!counter) {
    pending = 1;
    fcns.poll();
    counter = reload;       //"the next count rolls over to $ffff or auto-reloads"
    enable &= repeat;       //one-shot unless the repeat flag is set
  } else {
    counter--;
  }
}

auto FCNS::poll() -> void {
  cpu.irqLine(timer.pending);
}

//See the declaration in fcns.hpp for why bits 7 and 6 follow the hook and everything else is the
//measured idle value.
auto FCNS::status4126() const -> n8 {
  n8 data = Status4126Idle;
  if(cpu2.offHook()) { data.bit(7) = 0; data.bit(6) = 0; }
  //bit 2 is pin 69, the $40ac watchdog, reaching CPU2 on pin 49: it idles low and pulses high.
  //$e482 reports code $0c out the diagnostic port whenever it reads clear, which is every boot.
  data.bit(2) = watchdog.pulseActive();
  return data;
}

//Hardware addressing, matching both the nesdev wiki and Mesen's FnsMmc1.h: the glyph index comes
//straight from the $5xxx address, the 32 bytes of a glyph are contiguous, and $40b0 bit 0 picks the
//128KB bank. The dump we have is scrambled relative to this; that is corrected once at load time
//in cartridge/board/nintendo-fcns-bios.cpp, so this stays honest about what the chip does.
auto FCNS::kanjiAddress(n12 index, n1 bank, n5 counter) const -> u32 {
  return (u32)bank << 17 | (u32)index << 5 | (u32)counter;
}

auto FCNS::main() -> void {
  timer.clock();
  watchdog.clock();
  modem.clock();
  //The second dial tone is a physical thing on the line, so it advances on the machine's own clock --
  //one tick per CPU cycle. It used to ride the socket-poll divider below, which gave it 0.57ms
  //resolution; that is coarser than the cadence CPU2 can be asked to detect at a small timer divider,
  //and the tone came out as a few ragged steps instead of a pulse train.
  link.clock();
  link.clockDigits();
  //Link::poll() is a system call, so it stays divided down. 1024 cycles is about 1.7kHz -- far finer
  //than the modem's fastest bit rate, and cheap enough to be invisible. The counter is not
  //serialized: which cycle a socket is drained on is not machine state.
  if(!(++linkPollCounter & 0x3ff)) link.poll();
}

auto FCNS::power() -> void {
  mailbox = {};
  timer = {};
  mirror = 0;     //POR: vertical mirroring
  kanjiBank = 0;
  chrBank = 0;
  //$40ae bit 0 is documented with "POR value = 1", i.e. the built-in RAM's /CE starts enabled.
  cardRamEnable = 0;
  adapterRamEnable = 1;
  link.reset();
  modem.power();
  cpu2.power();
}

//$40a0-$40cf: RF5C66 registers. Addresses and bit meanings are the nesdev wiki's, cross-checked
//against which addresses the three real cards actually touch -- an opcode scan of all three finds
//$40a2/a3/a6/a7/a8/ab/ac/ad/ae/b0/b1/c0/c9/d0-d4 and never $40a0 or $40a1, which is how two
//earlier guesses at those two addresses were caught.
auto FCNS::read(n16 address, n8 data) -> n8 {
  //$40d4: the RF5A18's expansion pins and the tone receiver's output, as the Famicom sees them.
  //
  //  bit 0  pin 65 (Exp 17), open-drain -- I/O value
  //  bit 1  pin 67 (Exp 19), push-pull  -- output value
  //  bit 2  pin 66 (Exp 18), open-drain -- I/O value
  //  bits 3-7  pins 69-73, Tone Rx D1/D2/D4/D8/DV
  //
  //the tone receiver is the modem's DTMF decoder, so the Famicom can read incoming digits without
  //going through CPU2. nothing dials into the adapter here, so DV reads 0.
  if(address == 0x40d4) {
    data = 0x00;
    data.bit(0) = expansionOut.bit(0);
    data.bit(1) = expansionOut.bit(1);
    data.bit(2) = expansionOut.bit(2);
    data.bit(3) = modem.toneDigit.bit(0);
    data.bit(4) = modem.toneDigit.bit(1);
    data.bit(5) = modem.toneDigit.bit(2);
    data.bit(6) = modem.toneDigit.bit(3);
    data.bit(7) = modem.toneValid;
    return data;
  }

  //$40d5: clocks. Read-only, and the wiki has only bit 5 pinned -- "a 0.05% duty square wave, measured
  //1.200kHz" -- with bits 4-0 further divisions of the same crystal and bits 6-7 probably absent.
  //Modelled as square waves off the M2 cycle counter: bit 5 at 1.2kHz and each lower bit half that
  //again, which is the only arrangement consistent with "various clock outputs derived from the CPU2
  //crystal at different frequencies" and gives a card that polls one of them something that actually
  //moves. Frequencies below bit 5 are a guess in ratio, not in kind.
  if(address == 0x40d5) {
    data = 0x00;
    for(u32 bit : range(6)) {
      //1.2kHz at bit 5, halving downward: a half period is M2 / (2 * f)
      u64 halfPeriod = 1789773 / (2 * 1200) << (5 - bit);
    }
    return data;
  }

  //$40d6: the UART's status, as the Famicom sees it -- and every bit is INVERTED relative to what CPU2
  //reads, which is the whole reason this needs implementing rather than stubbing.
  //
  //  bit 1  !($4112.0)   0 = $4110 Rx buffer has a byte ready
  //  bit 2  !($4112.1)   0 = $4110 Tx buffer is writable
  //  bit 3  !($4113.1 OR $4113.2)
  //  bit 4  !($4113.7)   1 = transmit repeat
  //  bit 5  !($4113.6)
  //
  //so the card can watch the modem's byte flow directly.
  if(address == 0x40d6) {
    data = 0x00;
    data.bit(0) = 1;  //exists, function unknown; idles high with the rest
    data.bit(1) = !cpu2.uart.rxReady;
    data.bit(2) = !cpu2.uart.txReady;
    data.bit(3) = !(cpu2.uart.config2.bit(1) | cpu2.uart.config2.bit(2));
    data.bit(4) = !cpu2.uart.config2.bit(7);
    data.bit(5) = !cpu2.uart.config2.bit(6);
    data.bit(6) = 1;
    data.bit(7) = 1;
    return data;
  }

  //$40a5: the expansion port's inputs -- pins 9, 8, 7 and 11. Nothing is attached to those pins in any
  //configuration these cards were sold in, and they are pulled high.
  if(address == 0x40a5) return 0xff;
  //bit 7 = CIC authentication (every card's reset spins on `BIT $40c0 / BPL` before running any
  //further code); bit 3 = CHR RAM bank select; bit 0 = built-in RAM +CE.
  if(address == 0x40c0) {
    data = 0x00;
    data.bit(7) = fcnsCard.present;
    data.bit(3) = chrBank;
    data.bit(0) = adapterRamEnable;
    return data;
  }

  //$40a2: reading acknowledges /IRQ. bit 0 reports a timer interrupt.
  if(address == 0x40a2) {
    data = 0x00;
    data.bit(0) = timer.pending;
    timer.pending = 0;
    poll();
    return data;
  }

  //$40a6/$40a7: reads give the counter's present value, not the reload value.
  if(address == 0x40a6) return timer.counter.byte(0);
  if(address == 0x40a7) return timer.counter.byte(1);

  if(address == 0x40ad) { data = 0x00; data.bit(7) = mirror; return data; }
  if(address == 0x40ae) { data = 0x00; data.bit(0) = cardRamEnable; return data; }
  //$40ac: pin-69 watchdog keep-alive. Every observed access is a bare read (BIT) whose flags are
  //never tested by any of the three carts -- the read itself is what resets the 12.4892s countdown.
  //No value it could return is evidenced, so this returns open bus like the rest of the unmapped range.
  if(address == 0x40ac) { watchdog.reset(); return data; }
  //"Reading from $40b0 resets the Kanji auto-increment counter."
  if(address == 0x40b0) {
    kanjiCounter = 0;
    data = 0x00;
    data.bit(0) = kanjiBank;
    return data;
  }

  //reads return CPU2's latches, never the Famicom's own writes -- see fcns.hpp's Mailbox comment.
  if(address >= 0x40d0 && address <= 0x40d2) return mailbox.toFamicom[address - 0x40d0];
  if(address == 0x40d3) return mailbox.cpu2Flags;

  //$5000-$5fff: glyph index -> one byte of that glyph per read, advancing the counter. See the
  //kanjiCounter comment in fcns.hpp.
  if(address >= 0x5000 && address <= 0x5fff) {
    if(!kanjiROM) return data;
    if(kanjiCounter == 0) {
      kanjiIndex = (n12)address;  //latch; see fcns.hpp
      //When the card renders the first error glyph ($763 = the '障' of 障害), dump its return-address
      //stack: renderer <- error_display <- the routine that DECIDED to fault. Address-guessing has
      //failed repeatedly; this names the decision site directly.
    }
    return kanjiROM.read(kanjiAddress(kanjiIndex, kanjiBank, kanjiCounter++));
  }

  //$6000-$7fff is shared between the card's own PRG-RAM and the adapter's W-RAM -- only one may
  //be chip-enabled at a time (see fcns.hpp). If the game hasn't enabled either yet, open bus.
  if(address >= 0x6000 && address <= 0x7fff) {
    if(cardRamEnable && fcnsCard.present) return fcnsCard.read(address, data);
    if(adapterRamEnable) return wram.read(address - 0x6000);
    //With both RAMs disabled, JRA-PAT reads its cartridge EEPROM's serial DO here (bit 0); the card's
    //$a643 sets exactly this config before each read bit. See FCNSCard::EEPROM.
    if(fcnsCard.present) { data.bit(0) = fcnsCard.eeprom.out(); return data; }
    return data;
  }

  if(address >= 0x8000) {
    return fcnsCard.present ? fcnsCard.read(address, data) : data;
  }

  return data;
}

auto FCNS::write(n16 address, n8 data) -> void {
  //$40a6/$40a7 write the cycle counter's reload value.
  if(address == 0x40a6) { timer.reload.byte(0) = data; return; }
  if(address == 0x40a7) { timer.reload.byte(1) = data; return; }
  //$40a8: bit 0 repeat, bit 1 enable. "Writing anything to this register resets the cycle
  //counter to the reload value."
  if(address == 0x40a8) {
    timer.repeat = data.bit(0);
    timer.enable = data.bit(1);
    timer.counter = timer.reload;
    if(!timer.enable) { timer.pending = 0; poll(); }
    return;
  }
  //CPU2's reset line is driven from the Famicom side, and nesdev documents *two* controls with
  //opposite polarity because the hardware revisions differ: new units enable CPU2 with $40b1.3 = 0,
  //old ones with $40c0.2 = 1, and "to support all revisions when enabling CPU2, both of these bits
  //should be written".
  //
  //Deliberately not acted on. Driving reset from $40b1.3 alone reset CPU2 three extra times across
  //a single Super Mario Club session -- $40b1's low bits are modem pins, so ordinary writes there
  //look like reset requests. Requiring both controls to agree is worse still, because $40c0 is
  //written constantly for the CHR bank and RAM enable. Each spurious reset is unrecoverable: $f36b
  //destroys the $cf cold-reset marker and CPU2 then refuses every card request at $fb8d forever.
  //
  //Until it is known which revision this adapter is and which writes are genuinely reset requests,
  //letting CPU2 run is both closer to observed behaviour and far less destructive than guessing.

  //$40a4: expansion outputs, active low -- "EXP 5 = !($40A4.1)" and "EXP 4 = !($40A4.2)".
  if(address == 0x40a4) { expansionControl = data; return; }
  //$40d4 write: pin 65 (Exp 17, open-drain), pin 67 (Exp 19, push-pull), pin 66 (Exp 18, open-drain).
  //Nothing is wired to them here, so the value is latched and read back rather than acted on.
  if(address == 0x40d4) { expansionOut = data; return; }

  if(address == 0x40c0) { chrBank = data.bit(3); adapterRamEnable = data.bit(0); return; }

  //writes land in the Famicom's own latches, which CPU2 reads back at $4122-$4125.
  if(address >= 0x40d0 && address <= 0x40d2) { mailbox.toCPU2[address - 0x40d0] = data; return; }
  if(address == 0x40d3) {
    mailbox.famicomFlags = data;
    return;
  }

  if(address >= 0x6000 && address <= 0x7fff) {
    if(cardRamEnable && fcnsCard.present) { fcnsCard.write(address, data); return; }
    if(adapterRamEnable) { wram.write(address - 0x6000, data); return; }
    return;
  }

  if(address >= 0x8000) {
    if(fcnsCard.present) fcnsCard.write(address, data);
    return;
  }
}

//the swappable network card: PRG-ROM banking only, confirmed MMC1-shaped for JRA-PAT by the
//nesdev teardown (trimmed down from HVC_SxROM in cartridge/board/hvc-sxrom.cpp -- no CHR/mirror
//shift-register fields since this card has none of the variant hardware those exist for).
auto FCNSCard::load(Node::Object parent) -> void {
  port = parent->append<Node::Port>("Network Card Slot");
  port->setFamily("Famicom Network System");
  port->setType("Network Card");
  port->setHotSwappable(true);
  port->setAllocate([&](auto name) { return allocate(port); });
  port->setConnect([&] { return connect(); });
  port->setDisconnect([&] { return disconnect(); });
}

auto FCNSCard::unload() -> void {
  disconnect();
  port = {};
}

auto FCNSCard::allocate(Node::Port parent) -> Node::Peripheral {
  return node = parent->append<Node::Peripheral>("Famicom Network Card");
}

auto FCNSCard::connect() -> void {
  if(!node->setPak(pak = platform->pak(node))) return;

  information = {};
  information.title = pak->attribute("title");

  if(auto fp = pak->read("program.rom")) {
    programROM.allocate(fp->size(), 0xff);
    programROM.load(fp);
  }
  //size unconfirmed -- JRA-PAT's battery/EEPROM-adjacent SRAM, real chip likely much smaller
  //than the full $6000-7fff window, but this is a safe superset. See FCNS.md.
  programRAM.allocate(0x2000, 0xff);
  if(auto fp = pak->read("save.ram")) programRAM.load(fp);

  //JRA-PAT's BR93C46 EEPROM, persisted as its own 128-byte file (64 x 16-bit words, little-endian).
  //Kept separate from save.ram: on real hardware it is a distinct non-volatile chip, not the battery
  //SRAM. Absent (SMC/Nomura) it just stays blank and unused.
  eeprom.power();
  for(auto& w : eeprom.data) w = 0xffff;   //erased state
  if(auto fp = pak->read("save.eeprom")) {
    for(auto& w : eeprom.data) { n8 lo = fp->read(); n8 hi = fp->read(); w = hi << 8 | lo; }
  }

  shiftCount = 0;
  shiftValue = 0;
  programMode = 1;
  programSize = 1;
  programBank = 0;
  present = 1;
}

auto FCNSCard::disconnect() -> void {
  if(!node) return;
  save();
  programROM.reset();
  programRAM.reset();
  present = 0;
  node.reset();
}

auto FCNSCard::save() -> void {
  if(auto fp = pak->write("save.ram")) programRAM.save(fp);
  if(auto fp = pak->write("save.eeprom")) {
    for(auto w : eeprom.data) { fp->write(w & 0xff); fp->write(w >> 8); }
  }
}

auto FCNSCard::addressProgramROM(n32 address) -> n32 {
  bool region = address & 0x4000;
  n4 bank = programBank;
  if(programSize == 1) {
    bank = (region == 0 ? 0x0 : 0xf);
    if(region != programMode) bank = programBank;
  } else {
    bank = programBank & ~1 | region;
  }
  return bank << 14 | (n14)address;
}

auto FCNSCard::read(n16 address, n8 data) -> n8 {
  if(address >= 0x6000 && address <= 0x7fff) return programRAM.read(address - 0x6000);
  if(address >= 0x8000 && programROM) return programROM.read(addressProgramROM(address));
  return data;
}

auto FCNSCard::write(n16 address, n8 data) -> void {
  if(address >= 0x6000 && address <= 0x7fff) { programRAM.write(address - 0x6000, data); return; }
  if(address < 0x8000) return;

  if(data.bit(7)) {
    shiftCount = 0;
    programSize = 1;
    programMode = 1;
    return;
  }

  shiftValue = data.bit(0) << 4 | shiftValue >> 1;
  if(++shiftCount != 5) return;
  shiftCount = 0;
  //TEMP: is this really MMC1? Log which of the four register slots the card actually loads, and with
  //what, so an unemulated mapper shows up as traffic to a slot we treat as a no-op.

  switch(address >> 13 & 3) {
  case 0:
    programMode = shiftValue.bit(2);
    programSize = shiftValue.bit(3);
    //EEPROM chip-select rides the control register's low (mirroring) bit -- $a62e writes it via $8000
    //($1c | CS) just before clocking the CHR register. Latched here for the next CHR write.
    eepromCS = shiftValue.bit(0);
    break;
  case 1:
    //CHR bank 0 register clocks JRA-PAT's serial EEPROM (no CHR ROM is wired). Derived from the card's
    //bit-bang routines: CHR bit 0 = DI, CHR bit 1 = CLK; CS comes from the control register (above).
    eeprom.clock(eepromCS, shiftValue.bit(1), shiftValue.bit(0));
    break;
  case 3:
    programBank = shiftValue.bit(0,3);
    break;
  //case 2 (CHR bank 1) is a no-op here.
  }
}

//A BR93C46 (93C46) Microwire EEPROM in x16 mode. Clocked from the CHR-bank register on each 5-bit MMC1
//load. Frame after CS goes high: a start bit (1), a 2-bit opcode, a 6-bit address, then for WRITE 16
//data bits, or for READ 16 bits shifted out on DO. Opcodes with address top bits are EWEN/EWDS/etc.
auto FCNSCard::EEPROM::clock(n1 cs_, n1 clk_, n1 di_) -> void {
  n1 rising = clk_ && !clk;
  if(cs_ && !cs) { started = 0; bits = 0; shiftIn = 0; outCount = 0; }  //CS high: new frame
  cs = cs_; di = di_;
  clk = clk_;
  if(!cs) { doBit = 1; return; }                 //idle DO high while deselected
  if(!rising) return;                            //everything happens on the rising clock edge

  if(outCount) {                                 //READ: shift the loaded word out, MSB first
    doBit = outShift.bit(15);
    outShift = outShift << 1;
    if(--outCount == 0) started = 0;
    return;
  }
  if(!started) {                                 //wait for the leading start bit
    if(di) started = 1;
    return;
  }

  shiftIn = shiftIn << 1 | di; bits++;
  //A command header is opcode(2) + address(6) = 8 bits after the start bit.
  if(bits == 8) {
    n2 opcode = shiftIn >> 6 & 3;
    n8 addr   = shiftIn & 0x3f;
    if(opcode == 0b10) {                         //READ: load word, next 16 edges shift it out
      outShift = data[addr & 63]; outCount = 16; doBit = 0;
    } else if(opcode == 0b00) {                  //EWEN/EWDS/ERAL/WRAL by the two address top bits
      if((addr >> 4) == 0b11) writeEnable = 1;   //EWEN
      if((addr >> 4) == 0b00) writeEnable = 0;   //EWDS
      started = 0;
    } else if(opcode == 0b11) {                  //ERASE one word
      if(writeEnable) data[addr & 63] = 0xffff;
      started = 0;
    }
    //opcode 0b01 (WRITE): keep clocking, the next 16 bits are the data word
    return;
  }
  //After 24 bits the header sits in bits 23:16 and the data word in bits 15:0. opcode is bits 23:22.
  if(bits == 24 && (shiftIn >> 22 & 3) == 0b01) { //WRITE: 8 header + 16 data bits in
    if(writeEnable) data[shiftIn >> 16 & 0x3f] = shiftIn & 0xffff;
    started = 0;
  }
}

auto FCNSCard::EEPROM::power() -> void {
  cs = clk = di = 0; doBit = 1; writeEnable = 0;
  bits = 0; shiftIn = 0; outCount = 0; outShift = 0; started = 0;
}

}
