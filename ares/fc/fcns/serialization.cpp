auto FCNS::CPU2::serialize(serializer& s) -> void {
  WDC65C02S::serialize(s);
  Thread::serialize(s);
  s(ram);
  s(regs);
  s(resetPending);
  s(nmiTimer);
  s(irqTimer);
  s(uart);
  for(auto& shift : serialOut) { s(shift.shifter); s(shift.count); }
  s(serialInBit);
}

auto FCNS::serialize(serializer& s) -> void {
  s(present);
  s(cpu2);
  s(mailbox.toCPU2);
  s(mailbox.toFamicom);
  s(mailbox.famicomFlags);
  s(mailbox.cpu2Flags);
  s(characterRAM[0]);
  s(characterRAM[1]);
  s(wram);
  s(timer.counter);
  s(timer.reload);
  s(timer.enable);
  s(timer.repeat);
  s(timer.pending);
  watchdog.serialize(s);
  s(link);
  s(modem);
  s(mirror);
  s(kanjiBank);
  s(kanjiIndex);
  s(kanjiCounter);
  s(chrBank);
  s(cardRamEnable);
  s(adapterRamEnable);
}

auto FCNSCard::serialize(serializer& s) -> void {
  s(present);
  s(programROM);
  s(programRAM);
  s(shiftCount);
  s(shiftValue);
  s(programMode);
  s(programSize);
  s(programBank);
  eeprom.serialize(s);
}

auto FCNSCard::EEPROM::serialize(serializer& s) -> void {
  for(auto& w : data) s(w);
  s(cs); s(clk); s(di); s(doBit); s(writeEnable);
  s(bits); s(shiftIn); s(outCount); s(outShift); s(started);
}
