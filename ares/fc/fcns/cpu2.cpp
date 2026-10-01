//RF5A18 CPU2: a WDC65C02S running the dumped internal ROM, entirely independent of the
//Famicom's own 6502. See fcns.hpp for the confirmed mailbox address mapping.

//Counting down at the CPU2 clock, firing at zero, and either reloading or stopping. The FDS timer
//this is modelled on behaves the same way, and $e12a's count of six only makes sense as cycles.
auto FCNS::CPU2::Timer::clock() -> void {
  if(!running) return;
  if(++prescaler < divider) return;  //timer 1 counts 2048 CPU2 cycles to the count; timer 2, one
  prescaler = 0;
  if(counter--) return;
  counter = reload;
  pending = 1;
  if(!loop) running = 0;
}

auto FCNS::CPU2::Timer::control(n8 data) -> void {
  loop = data.bit(0);
  if(!data.bit(1)) {
    running = 0;
    pending = 0;  //stopping the timer withdraws the request; the FDS timer does this too
    return;
  }
  counter = reload;
  prescaler = 0;
  //arming clears any request left over from a previous expiry. Without this a one-shot that fired
  //while its interrupt could not be taken leaves `pending` set with `running` clear, and the next
  //arm starts from an already-asserted line. The FDS timer this is modelled on has the same shape:
  //its $4022 path clears pending whenever the interrupt is not being enabled.
  pending = 0;
  running = 1;
}

//reading the status register is the acknowledge. What it reads back is not established, so it
//returns the flag it is clearing rather than inventing a value: the ROM only ever does LDX $4103 /
//LDX $4107 and discards the result.
auto FCNS::CPU2::Timer::acknowledge() -> n8 {
  n8 data = pending;
  pending = 0;
  return data;
}

auto FCNS::CPU2::Timer::serialize(serializer& s) -> void {
  s(prescaler); s(reload); s(counter); s(loop); s(running); s(pending);
}

//NMI is edge-triggered and IRQ is level-triggered, so the NMI timer's flag is consumed when the
//interrupt is taken while the IRQ timer's is held until the ROM reads $4107.
auto FCNS::CPU2::irqPending() -> bool {
  //$412f bit 7 enables the UART receive interrupt; a byte waiting in $4110 is what raises it, and
  //reading $4110 is what clears it.
  if(uart.rxReady && uartIrqEnable() && !P.i) return true;
  return (irqTimer.pending && irqTimerEnable() && !P.i) || (nmiTimer.pending && nmiTimerEnable());
}

auto FCNS::CPU2::nmi(n16& vector) -> void {
  if(nmiTimer.pending && nmiTimerEnable()) {
    nmiTimer.pending = 0;
    vector = 0xfffa;
  }
}

auto FCNS::CPU2::lastCycle() -> void {
  //nothing to latch: irqPending() is read directly by the base component
}

auto FCNS::CPU2::power() -> void {
  WDC65C02S::power();
  for(auto& r : regs) r = 0x00;
  nmiTimer = {};
  irqTimer = {};
  uart.power();
  for(auto& shift : serialOut) shift = {};
  serialInBit = 0;
  //timer 1 counts 2048 CPU2 cycles to the count. the dial tables confirm it twice over: the wiki's
  //millisecond annotations match 2048 and nothing else, and the $e929 gap table works out to 10.2 and
  //20.7 pulses per second, the two standard dial rates $4126 bits 3/4 select.
  nmiTimer.divider = 2048;
  irqTimer.divider = 1;     //$4104-$4107 counts one
  //WDC65C02S::power() only clears the registers -- it does not fetch the reset vector. That is a
  //separate reset(), which has to be driven on the first main() iteration the way gameking's CPU
  //does it (ares/gameking/cpu/cpu.cpp). Without this CPU2 begins executing at $0000 instead of
  //$f36f and immediately runs away through RAM.
  resetPending = 1;
  create(2'457'600.0, std::bind_front(&FCNS::CPU2::main, this));
}

auto FCNS::CPU2::unload() -> void {
  destroy();
}

auto FCNS::CPU2::main() -> void {
  //$40b1 bit 3 holds CPU2 in reset from the Famicom side; while asserted it does not execute.
  if(resetPending) {
    resetPending = 0;
    return reset();
  }
  //STP halts until a real reset; WAI idles until an interrupt from one of the on-chip timers.
  if(stopped || waiting) return idle();
  if(irqPending()) {
    return interrupt();
  }

  instruction();
}

auto FCNS::CPU2::step() -> void {
  nmiTimer.clock();
  irqTimer.clock();
  uart.step();
  Thread::step(1);
  Thread::synchronize(cpu);
}

auto FCNS::CPU2::read(n16 address) -> n8 {
  n8 data = 0x00;

  if(address <= 0x1fff) {
    data = ram.read(address & 0x1fff);
    //$e98e is `LDA $30` -- the digit about to go to TR -- so PC is $e990 on the data read. $e9d5 is
    //`LDA $0202,x`, the script fetch, leaving PC at $e9d8.
    //DIAGNOSTIC: $e8c3 `LDA $0201 / BNE $e917` skips the whole 16-tick pre-check when the third
    //header byte is non-zero. Ours is $00. Forcing it tests whether that pre-check is what makes
    //the card give up at divider 2048, and whether it has any bearing on the index-32 stall.
  } else if(address >= 0xe000) {
    if(rom) data = rom.read(address - 0xe000);
    //$e999 `LDA $e929,y` and $e99c `LDX $e92a,y` fetch the post-digit delay from a table; PC lands
    //at $e99c and $e99f respectively. The table only runs $e920-$e932, so an index past its end
    //reads code as a timer reload -- which would arm a timer that never fires.
  } else if(address >= 0x4100 && address <= 0x413f) {
    switch(address) {
    //mirror of the Famicom side: CPU2's reads return the FAMICOM's latches, not its own writes.
    case 0x4120: case 0x4121: data = fcns.modem.read(address); break;
    case 0x4103:
      data = nmiTimer.acknowledge();
      //$e825 `LDA $4103` is the first thing the NMI handler does, so PC is $e828 here. Log the
      //dispatch state for the ticks around where the script walk dies.
      break;
    case 0x4110: case 0x4111: case 0x4112: case 0x4113: case 0x4114: data = uart.read(address); break;
    //$4135 bit 0 = "$4134 Serial Data Ready". Nothing drives the serial input, so it never becomes
    //ready; reporting otherwise would make the ROM's $ffb8 loop accumulate 40 bytes of nothing.
    case 0x4135: data = 0x00; break;
    //"Without any other register reads/writes, the next bit is provided the next time this register
    //is read" -- so the bit position advances here and is reset by every other $41xx access below.
    case 0x4134:
      data = 0x00;  //nothing is driving the input; idles low
      serialInBit++;
      return step(), data;
    //input status. bit 2 is pin 49 from the RF5C66; bits 3 and 4 are switches SW1-2 and SW1-4,
    //read once at the first dial-script byte to pick the dialling mode -- 697.5ms or 547.5ms per
    //digit by pulsing the hook relay, or 158.3ms through the modem's DTMF generator.
    case 0x4126: data = fcns.status4126(); break;
    case 0x4107: data = irqTimer.acknowledge(); break;
    case 0x4122: data = fcns.mailbox.famicomFlags; break;
    case 0x4123: data = fcns.mailbox.toCPU2[0]; break;
    case 0x4124: data = fcns.mailbox.toCPU2[1]; break;
    case 0x4125: data = fcns.mailbox.toCPU2[2]; break;
    default: data = regs[address - 0x4100]; break;
    }
    serialInBit = 0;
  }

  step();
  return data;
}

auto FCNS::CPU2::write(n16 address, n8 data) -> void {
  if(address <= 0x1fff) {
    ram.write(address & 0x1fff, data);
  } else if(address >= 0x4100 && address <= 0x413f) {
    switch(address) {
    //mirror image of fcns.cpp's Famicom-side $40d3 write: CPU2 claiming/releasing the mailbox.
    case 0x4122:
      //$fc05, $fa7b and $fbfd/$fc00 each write $e0 and then JMP $f36b -- a soft reset, which is
      //fatal because it destroys the $cf cold-reset marker. Catch which one fires.
      if(PC == 0xfc05 || PC == 0xfa7b || PC == 0xfc08 || PC == 0xfbfe) {
      }
      if(data != fcns.mailbox.cpu2Flags) {
      }
      fcns.mailbox.cpu2Flags = data;
      break;
    case 0x4100: nmiTimer.reload.byte(0) = data; break;
    case 0x4101: nmiTimer.reload.byte(1) = data; break;
    case 0x4102: nmiTimer.control(data); break;
    case 0x4104: irqTimer.reload.byte(0) = data; break;
    case 0x4105: irqTimer.reload.byte(1) = data; break;
    case 0x4106: irqTimer.control(data); break;
    case 0x4110: case 0x4111: case 0x4112: case 0x4113: case 0x4114:
      uart.write(address, data); uart.trace(address, data); break;
    //$4136 starts a serial transmission and $4137 clears the buffers before one; both are written with
    //$00. The shift registers themselves are in serialOut[], so starting means committing whatever has
    //been shifted in and clearing means dropping it.
    case 0x4136:
      for(auto& shift : serialOut) {
        shift = {};
      }
      break;
    case 0x4137:
      for(auto& shift : serialOut) shift = {};
      serialInBit = 0;
      break;
    case 0x4130: case 0x4131: case 0x4132: case 0x4133: {
      auto& shift = serialOut[address - 0x4130];
      shift.shifter = shift.shifter << 1 | data.bit(7);  //bit 7 first, as the ROM's ROL loop implies
      if(!++shift.count) {
      }
      break;
    }
    case 0x4120: case 0x4121:
      //log who is feeding the modem: the byte written to $4121 is the register payload, and for a
      //dial digit that is the TR value. The PC names the routine the digit came from.
      fcns.modem.write(address, data);
      break;
    case 0x4123:
      fcns.mailbox.toFamicom[0] = data;
      break;
    case 0x4124: fcns.mailbox.toFamicom[1] = data; break;
    case 0x4125: fcns.mailbox.toFamicom[2] = data;
      break;
    default:
      //$412f bit 0 gates the NMI timer. If it is ever cleared and not restored, every later arm
      //fires into a disabled line, pending sticks, and the state machine stops for good -- which is
      //exactly the observed "chain dies after N re-arms".
      regs[address - 0x4100] = data;
      break;
    }
    serialInBit = 0;
  }

  step();
}
