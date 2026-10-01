//Famicom Network System: Nintendo's dial-up modem adapter, used by JRA-PAT, Nomura no Famicom
//Trade and Super Mario Club. The adapter holds an RF5C66 (mapper, mailbox, M2 counter, kanji ROM
//addressing), an RF5A18 containing a second CPU (a WDC65C02S running a dumped internal ROM), an
//Oki MSM6827L modem, and a 256KB LH5323M1 kanji ROM.
//
//The two CPUs talk through one four-byte mailbox, dual-ported at different offsets on each bus:
//
//   Famicom side   $40d0  $40d1  $40d2  $40d3
//   CPU2 side      $4123  $4124  $4125  $4122
//
//$40d0-$40d2 are three shared data bytes and $40d3 a handshake byte, each direction latched
//separately: a side reads what the other wrote, never its own writes. Both ROMs poll the handshake
//with BIT, testing bits 7 and 6, so the protocol is left to emerge from the two CPUs running
//against shared storage rather than being reimplemented here.

struct FCNS {
  n1 present = 0;
  Node::Object node;

  //a trace line for everything crossing the network interface: card requests, modem programming,
  //and bytes in or out of the link.
  struct Debugger {
    auto load(Node::Object) -> void;
    auto unload(Node::Object) -> void;
    auto network(string message) -> void;
    struct Tracer { Node::Debugger::Tracer::Notification network; } tracer;
  } debugger;

  struct CPU2 : WDC65C02S, Thread {
    //fcns.cpp
    auto main() -> void;
    auto power() -> void;
    auto unload() -> void;

    //memory.cpp
    auto read(n16 address) -> n8 override;
    auto write(n16 address, n8 data) -> void override;
    auto step() -> void;

    auto lastCycle() -> void override;
    auto irqPending() -> bool override;
    auto nmi(n16& vector) -> void override;

    //two on-chip timers: $4100-$4103 drives NMI, $4104-$4107 drives IRQ. control is bit 0 = loop
    //and bit 1 = restart; reading the status register clears that timer's pending flag.
    //they do not tick at the same rate: timer 1 counts 2048 CPU2 cycles to the count, timer 2 one.
    struct Timer {
      auto clock() -> void;
      auto control(n8 data) -> void;
      auto acknowledge() -> n8;
      auto serialize(serializer&) -> void;

      u32 divider = 1;  //CPU2 cycles per count: 2048 for timer 1, 1 for timer 2
      u32 prescaler;
      n16 reload;
      n16 counter;
      n1  loop;
      n1  running;
      n1  pending;
    } nmiTimer, irqTimer;

    //$412f enables interrupts: bit 0 timer 1 NMI, bit 6 timer 2 IRQ, bit 7 UART receive.
    //$4110-$4114 is the on-chip serial port, and the data path; $4120/$4121 only configure the
    //modem's registers and never carry a payload byte.
    struct UART {
      auto read(n16 address) -> n8;
      auto write(n16 address, n8 data) -> void;
      auto trace(n16 address, n8 data) -> void;
      auto step() -> void;
      auto power() -> void;
      auto serialize(serializer&) -> void;

      auto baud() const -> u32;
      auto frameBits() const -> u32;
      auto framePeriod() const -> u32;

      n8 config;      //$4111
      n8 config2;     //$4113
      n8 baudSelect;  //$4114
      n8 rxData, txData, rxShift;
      n1 rxReady, txReady, txIdle;
      n1 parityError, framingError, breakReceived;
      u32 txCounter, rxCounter;
    } uart;

    //$4130-$4133 shift one bit out per access and $4134 one bit in, most significant bit first.
    //what they connect to is unknown, so they are modelled as shift registers and input idles.
    struct SerialShift {
      n8  shifter;
      n3  count;
    } serialOut[4];
    n3 serialInBit;

    auto nmiTimerEnable() const -> bool { return regs[0x2f].bit(0); }
    auto irqTimerEnable() const -> bool { return regs[0x2f].bit(6); }
    auto uartIrqEnable()  const -> bool { return regs[0x2f].bit(7); }

    Memory::Readable<n8> rom;  //$e000-$ffff, the dumped RF5A18 internal ROM (8KB)
    //8KB, and not a mirrored 4KB: the reset routine clears $0000-$1fff a page at a time and then
    //reads its cold-reset marker back out of $00ff. Folded onto 4KB, the page-$10 pass writes $10ff
    //over $00ff, the marker reads back as $00 instead of $cf, and CPU2 spends the rest of the run in
    //its error-recovery paths ($fd9e, and $fb8d refusing every card request).
    Memory::Writable<n8> ram;  //$0000-$1fff, 8KB internal RAM
    n8 regs[0x40];             //$4100-$413f catch-all latch storage

    //$4127 is the output control port, all active low: bit 1 /red LED, bit 2 /green LED, bit 4
    ///phone off hook, bit 5 /DTMF output enable, bit 6 /phone audio enable. pulse dialling clicks
    //bit 4 open and shut, which is the hook switch breaking the loop once per pulse.
    auto offHook()  const -> bool { return !regs[0x27].bit(4); }
    auto redLED()   const -> bool { return !regs[0x27].bit(1); }
    auto greenLED() const -> bool { return !regs[0x27].bit(2); }
    auto dtmfEnabled() const -> bool { return !regs[0x27].bit(5); }
    n1 resetPending;           //drives the reset-vector fetch on the first main() iteration

    //serialization.cpp
    auto serialize(serializer&) -> void;
  } cpu2;

  //each direction is latched separately: a side's reads return the other side's writes, never its
  //own. both idle at $ff. bits 7 and 6 are the handshake pair each side polls with BIT.
  struct Mailbox {
    n8 toCPU2[3] = {0xff, 0xff, 0xff};      //Famicom writes $40d0-$40d2, CPU2 reads $4123-$4125
    n8 toFamicom[3] = {0xff, 0xff, 0xff};   //CPU2 writes $4123-$4125, Famicom reads $40d0-$40d2
    //Bit 6 of the card's flag byte is an abort request: CPU2's wait loops at $fc0c/$fc26 test it
    //with BVS and, if set, soft-reset themselves through $fbfe -> $f36b with reason 3. Powering this
    //latch high therefore made CPU2 abort on its very first wait, before the card had said anything.
    n8 famicomFlags = 0x00;                 //Famicom writes $40d3; CPU2 reads it at $4122
    n8 cpu2Flags = 0xff;                    //CPU2 writes $4122; Famicom reads it at $40d3
  } mailbox;


  //RF5C66 M2 cycle counter ($40a6/$40a7 reload, $40a8 control, $40a2 acknowledge). there is no
  //scanline IRQ, so games build raster splits out of this. same shape as the FDS timer.
  struct Timer {
    n16 counter;
    n16 reload;
    n1  enable;   //$40a8 bit 1
    n1  repeat;   //$40a8 bit 0
    n1  pending;  //$40a2 bit 0, cleared by reading $40a2

    //fcns.cpp
    auto clock() -> void;
  } timer;

  //RF5C66 pin 69: an ~11.9085us pulse whenever $4xac has not been read for ~12.4892s, repeating
  //every further 12.4892s. both are exact powers of the 21.47727MHz crystal (2^28 and 2^8 cycles),
  //which is 12x the Famicom CPU clock. the cards poke $4xac with a bare BIT, so the read itself is
  //the keep-alive; Nintendo's patent JPH0233239A describes the pulse as forcing the line down.
  struct Watchdog {
    u64 cycles;  //RF5C66-crystal cycles since the last $4xAC read

    auto clock() -> void { if(cycles < (1ull << 29)) cycles += 12; }
    auto reset() -> void { cycles = 0; }
    auto pulseActive() const -> bool {
      if(cycles < (1ull << 28)) return false;
      return ((cycles - (1ull << 28)) % (1ull << 28)) < (1ull << 8);
    }
    auto serialize(serializer& s) -> void { s(cycles); }
  } watchdog;

  //The seam to the outside world: a replacement server, a companion script, or another emulator.
  //See link.cpp for why the modem talks to this instead of to an analogue phone line.
  //socket.cpp
  struct Socket {
    auto open(string target) -> bool;
    auto close() -> void;
    auto opened() const -> bool { return handle >= 0; }
    auto write(const u8* data, u32 size) -> void;
    auto read(u8* data, u32 size) -> u32;

    s64 handle = -1;
  };

  //$4126 is read-only status. idle is $fb, as returned by the system revision reply ($03) on real
  //hardware. bits 7 and 6 are line state: set on hook, clear once the line is seized ($e8fc waits
  //for them to clear). bit 2 is pin 69, the $40ac watchdog, which idles low. bits 4 and 3 are the
  //dialling-mode switches and bit 5 must read set, or the dial gives up with status $02.
  static constexpr u8 Status4126Idle = 0xfb;
  auto status4126() const -> n8;

  u32 linkPollCounter;

  struct Link {
    enum class Mode : u32 { Disconnected, Loopback, Socket };
    static constexpr u32 QueueLimit = 4096;

    Mode mode = Mode::Disconnected;
    //where to reach the peer standing in for the telephone network: "loopback", or a host:port.
    //Set through ares::Famicom::option("Network Link"); empty leaves the line dead.
    string target;
    std::vector<n8> tx, rx;
    FCNS::Socket socket;
    string pending;  //partial line from the peer, awaiting its newline
    n1 answered;     //the far end picked up
    n1 busy;         //the far end refused the call
    n1 polarity;     //line polarity reversed: NTT's answer supervision signal

    //NTT second dial tone. The DDX network answers the access number with an interrupted 400Hz
    //tone, and the card waits for it before sending the DDX address -- so it is a pulse train, not
    //a level. Defaults are SMC-Server's SDT_DEFAULTS (on 8, off 12, 36 cycles, in 10ms units).
    u32 sdtOn, sdtOff, sdtCycles, sdtTicks;   //on/off in milliseconds
    u32 digitsSent;        //dial digits since the call was placed
    u32 digitGap = ~0u;    //cycles since the last one; ~0 means the timeout has already fired
    auto clockDigits() -> void;
    static auto sdtTicksFor(u32 milliseconds) -> u32;
    auto clock() -> void;
    auto playing() const -> bool;

    auto open() -> void;
    auto connected() const -> bool;
    auto offHook() const -> bool;
    auto poll() -> void;
    auto send(string line) -> void;
    auto parse(string line) -> void;
    auto digit(n8 value) -> void;
    auto hangup() -> void;
    auto secondDialTone() const -> bool;
    auto reversed() const -> bool { return polarity; }
    auto transmit(n8 data) -> void;
    auto receivable() const -> bool;
    auto receive() -> n8;
    auto reset() -> void;
    auto serialize(serializer&) -> void;
  } link;

  //Oki MSM6827L modem, driven by CPU2 bit-banging $4120/$4121. See msm6827.cpp.
  struct Modem {
    n8 cr0, cr1, tr;     //operation mode / option / tone registers (DR is read-only, computed)
    n8 latch;            //the byte being shifted in or out
    n8 control;          //last value written to $4120
    n1 wrAsserted, rdAsserted;
    n1 carrierDetect, answerTone, callProgress, longLoop, receiveData, rdlRequest;
    //The MSM6827's DTMF receiver, which the Famicom reads directly through $40d4 bits 3-7 (D1/D2/D4/D8
    //and DV) as well as CPU2 through $4128. Nothing dials *into* the adapter here, so DV stays clear
    //and the data nybble idles -- an incoming digit would be a caller on the line, which there is not.
    n4 toneDigit;
    n1 toneValid;
    n1 dialStarted;  //dial tone ends at the first digit, as on a real line
    //Dial tone does not appear the instant the handset lifts: the exchange takes a moment. That
    //delay is what gives $42's DR-change wait a real rising edge to catch, instead of a level that
    //was already high before it started polling.
    n1  tonePresent;
    n8  toneState;   //bring-up: last logged (offHook, dialTone, dialStarted, sdt)
    u32 toneDelay;

    static auto dtmfKey(n4 data) -> char;
    auto registerIndex(n8 control) const -> n2;
    auto status() const -> n8;
    auto read(n16 address) -> n8;
    auto write(n16 address, n8 data) -> void;
    auto clock() -> void;
    auto power() -> void;
    auto serialize(serializer&) -> void;
  } modem;

  n1 mirror;     //$40ad bit 7: 0 = vertical (CIRAM A10 = PPU A10), 1 = horizontal (= PPU A11)
  //$5000-$5fff addresses a glyph index, not a byte: each glyph is 32 bytes (16x16 1bpp) read out
  //one per access through an auto-increment counter. 4096 indices x 32 bytes x 2 banks = 256KB.
  //reading $40b0 resets the counter. the index is latched on the first read after that reset and
  //the next 31 reads serve the same glyph, whatever $5xxx address they use.
  n1  kanjiBank;     //$40b0 bit 0, selects which 128KB bank
  n12 kanjiIndex;    //latched glyph index
  n5  kanjiCounter;  //byte position within the current 32-byte glyph
  n1 chrBank;    //$40c0 bit 3, selects between the two 8KB CHR RAM chips
  //$40a4 expansion outputs (active low: EXP5 = !bit 1, EXP4 = !bit 2) and $40d4's three expansion
  //pins. Nothing is attached to any of them in the configurations these cards shipped in, so both are
  //latched and read back rather than driving anything.
  n8 expansionControl;
  n8 expansionOut;

  Memory::Readable<n8> kanjiROM;  //256KB LH5323M1

  //CHR-RAM: 16KB as two 8KB chips, on the adapter. the card has no PPU connection, and this RAM is
  //not reachable from the CPU address space: it is written the ordinary way, through $2006/$2007.
  Memory::Writable<n8> characterRAM[2];

  //$6000-$7fff: the adapter's own 8KB work RAM, sharing the window with the card's PRG-RAM. only
  //one may be chip-enabled at a time, through $40ae bit 0 and $40c0 bit 0; the card manages that.
  Memory::Writable<n8> wram;

  n1  cardRamEnable;     //$40ae bit 0
  n1  adapterRamEnable;  //$40c0 bit 0 (independent of bit 7's CIC-ish boot gate)

  //fcns.cpp
  auto load() -> void;
  auto unload() -> void;
  auto power() -> void;
  auto main() -> void;
  auto poll() -> void;
  auto kanjiAddress(n12 index, n1 bank, n5 counter) const -> u32;

  auto read(n16 address, n8 data) -> n8;
  auto write(n16 address, n8 data) -> void;

  //serialization.cpp
  auto serialize(serializer&) -> void;
};

extern FCNS fcns;

//the swappable "tsuushin" game card (JRA-PAT, Nomura no Famicom Trade, Super Mario Club),
//inserted through its own hot-swappable port the same way FDS disks are -- see
//reference/FCNS.md for why this isn't part of the adapter's own pak. JRA-PAT's card is
//confirmed MMC1 (MMC1B2F) by the nesdev teardown; Nomura/Super Mario Club are assumed the same
//for now and may need their own board logic once they're confirmed booting or not.
struct FCNSCard {
  Node::Port port;
  Node::Peripheral node;
  VFS::Pak pak;
  n1 present = 0;

  auto title() const -> string { return information.title; }

  //no CHR/PPU wiring at all -- confirmed on nesdev: FNS cards have no PPU bus connection, CHR-RAM
  //lives on the adapter (see FCNS::characterRAM above), not here.
  Memory::Readable<n8> programROM;
  Memory::Writable<n8> programRAM;

  //fcns.cpp
  auto load(Node::Object) -> void;
  auto unload() -> void;

  auto allocate(Node::Port) -> Node::Peripheral;
  auto connect() -> void;
  auto disconnect() -> void;
  auto save() -> void;

  auto read(n16 address, n8 data) -> n8;
  auto write(n16 address, n8 data) -> void;

  //serialization.cpp
  auto serialize(serializer&) -> void;

  //bring-up: FCNS::write() reports which bank was live when the card armed a mailbox transfer, so a
  //logged PC can be resolved to a ROM offset. Remove with the rest of the bring-up probes.
  friend struct FCNS;

  //JRA-PAT's cartridge carries a BR93C46 -- a standard 1kbit Microwire serial EEPROM (64 x 16-bit),
  //storing the DDX server phone number and login. Its boot self-test ($a665 in the card ROM) writes
  //$55/$aa through it and reads them back; a card without a working EEPROM is declared 通信カートリッジ
  //不良. The EEPROM is driven off the MMC1 CHR-bank register (there is no CHR ROM, so those pins are
  //free): derived from the ROM's bit-bang routines ($a60b/$a62e/$f2cf/$a643),
  //   CHR bit 0 = DI, CHR bit 1 = CLK; CS = control-register bit 0; DO is read back at $6000 bit 0 with the SRAM
  //   disabled ($40ae=0, $40c0.0=0). Standard 93C46 opcodes: READ (10), WRITE (01), EWEN/EWDS/ERASE.
  struct EEPROM {
    auto clock(n1 cs, n1 clk, n1 di) -> void;   //driven from the CHR register each write
    auto out() const -> n1 { return doBit; }
    auto power() -> void;
    auto serialize(serializer&) -> void;

    n16 data[64];      //64 words of 16 bits
    n1  cs, clk, di, doBit = 1;
    n1  writeEnable;
    n8  bits;          //bits shifted in this frame
    n32 shiftIn;       //command+address+data being clocked in
    n8  outCount;      //remaining data bits to shift out on READ
    n16 outShift;      //data being shifted out
    n1  started;       //saw the leading start bit after CS
  } eeprom;

private:
  auto addressProgramROM(n32 address) -> n32;

  n8 shiftCount;
  n5 shiftValue;
  n1 eepromCS;   //EEPROM chip-select, latched from the control register for the CHR-write clock
  n1 programMode;
  n1 programSize;
  n4 programBank;

  struct Information {
    string title;
  } information;
};

extern FCNSCard fcnsCard;
