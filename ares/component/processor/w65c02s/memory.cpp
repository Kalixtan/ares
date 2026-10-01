auto WDC65C02S::idle() -> void {
  read(PC);
}

auto WDC65C02S::idleZeroPage(n8 addr) -> void {
  read(addr);
}

//NMOS guesses a bogus (old-page, new-low) address for this dummy cycle;
//65C02 fixes the glitch by just re-reading the operand byte it last fetched
//from the instruction stream (confirmed against SingleStepTests' wdc65c02
//corpus: the observed address is always PC-1, not any data-derived address).
auto WDC65C02S::idlePageCrossed(n16 x, n16 y) -> void {
  if(x >> 8 == y >> 8) return;
  read(PC - 1);
}

auto WDC65C02S::idlePageAlways(n16 x, n16 y) -> void {
  read(PC - 1);
}

auto WDC65C02S::opcode() -> n8 {
  return read(PC++);
}

auto WDC65C02S::operand() -> n8 {
  return read(PC++);
}

auto WDC65C02S::load(n8 addr) -> n8 {
  return read(addr);
}

auto WDC65C02S::store(n8 addr, n8 data) -> void {
  write(addr, data);
}

auto WDC65C02S::push(n8 data) -> void {
  write(0x0100 | S--, data);
}

auto WDC65C02S::pull() -> n8 {
  return read(0x0100 | ++S);
}
