auto WDC65C02S::addressAbsolute() -> n16 {
  n16 absolute = operand();
  absolute |= operand() << 8;
  return absolute;
}

//the reserved "nop_c_aba"/"nop_s_abx" opcodes ($5C/$DC/$FC): despite being
//nominally absolute or absolute,X addressed, real hardware never reads the
//effective address at all -- it fetches both operand bytes and then idles
//once on the just-fetched high-byte position (confirmed against
//SingleStepTests' wdc65c02 corpus: always exactly 4 cycles, X has no effect).
auto WDC65C02S::addressAbsoluteNop() -> n16 {
  n16 absolute = operand();
  absolute |= operand() << 8;
  read(PC - 1);
  return absolute;
}

auto WDC65C02S::addressAbsoluteJMP() -> n16 {
  n16 absolute = operand();
  // JMP absolute last cycle
L absolute |= operand() << 8;
  return absolute;
}

//JMP (abs,X): reads the target from (operand16 + X), fully wrapping across
//pages since the pointer is computed as a real 16-bit sum, not built byte by
//byte. Six cycles: opcode, two operand reads, one idle for the index add,
//two pointer reads.
auto WDC65C02S::addressAbsoluteIndirectX() -> n16 {
  n16 absolute = operand();
  absolute |= operand() << 8;
  read(PC - 2);  //dummy re-read of the absolute address' low byte position
  n16 pointer = absolute + X;
  n16 indirect = read(pointer);
  // JMP (abs,X) last cycle
L indirect |= read(pointer + 1) << 8;
  return indirect;
}

auto WDC65C02S::addressAbsoluteXRead() -> n16 {
  n16 absolute = operand();
  absolute |= operand() << 8;
  idlePageCrossed(absolute, absolute + X);
  return absolute + X;
}

auto WDC65C02S::addressAbsoluteXWrite() -> n16 {
  n16 absolute = operand();
  absolute |= operand() << 8;
  idlePageAlways(absolute, absolute + X);
  return absolute + X;
}

auto WDC65C02S::addressAbsoluteYRead() -> n16 {
  n16 absolute = operand();
  absolute |= operand() << 8;
  idlePageCrossed(absolute, absolute + Y);
  return absolute + Y;
}

auto WDC65C02S::addressAbsoluteYWrite() -> n16 {
  n16 absolute = operand();
  absolute |= operand() << 8;
  idlePageAlways(absolute, absolute + Y);
  return absolute + Y;
}

auto WDC65C02S::addressImmediate() -> n16 {
  return PC++;
}

auto WDC65C02S::addressImplied() -> n16 {
  return PC;
}

//fixes the NMOS JMP (abs) page-wrap bug: the pointer is incremented as a real
//16-bit value, so a pointer ending in $xxFF correctly carries into the next
//page instead of wrapping within it. costs one extra cycle over the NMOS
//form (six total instead of five) to do that carry.
auto WDC65C02S::addressIndirect() -> n16 {
  n16 absolute = operand();
  absolute |= operand() << 8;
  n16 indirect = read(absolute);
  //JMP (abs) still performs the NMOS same-page-wraparound bus read for the
  //pointer's high byte (for cycle-timing compatibility), but that read's
  //value is discarded: the added 65C02 cycle re-reads the correctly
  //incremented, non-wrapped address, and it's THAT value which is actually
  //used for the jump (confirmed against SingleStepTests' wdc65c02 corpus).
  read((absolute & 0xff00) | (absolute + 1 & 0x00ff));
L indirect |= read(absolute + 1) << 8;
  return indirect;
}

auto WDC65C02S::addressIndirectX() -> n16 {
  n8 zeroPage = operand();
  idleZeroPage(zeroPage);
  n16 absolute = load(zeroPage + X);
  absolute |= load(zeroPage + X + 1) << 8;
  return absolute;
}

auto WDC65C02S::addressIndirectYRead() -> n16 {
  n16 zeroPage = operand();
  n16 absolute = load(zeroPage + 0);
  absolute |= load(zeroPage + 1) << 8;
  idlePageCrossed(absolute, absolute + Y);
  return absolute + Y;
}

auto WDC65C02S::addressIndirectYWrite() -> n16 {
  n8 zeroPage = operand();
  n16 absolute = load(zeroPage + 0);
  absolute |= load(zeroPage + 1) << 8;
  idlePageAlways(absolute, absolute + Y);
  return absolute + Y;
}

auto WDC65C02S::addressRelative() -> n16 {
  return PC++;
}

auto WDC65C02S::addressZeroPage() -> n16 {
  return operand();
}

//the new 65C02 (zp) addressing mode: an indirect zero-page pointer with no
//index. fills the eight opcode slots that were JAM on NMOS (ORA/AND/EOR/ADC/
//STA/LDA/CMP/SBC).
auto WDC65C02S::addressZeroPageIndirect() -> n16 {
  n8 zeroPage = operand();
  n16 absolute = load(zeroPage + 0);
  absolute |= load(zeroPage + 1) << 8;
  return absolute;
}

auto WDC65C02S::addressZeroPageX() -> n16 {
  n8 zeropage = operand();
  idleZeroPage(zeropage);
  zeropage += X;
  return zeropage;
}

auto WDC65C02S::addressZeroPageY() -> n16 {
  n8 zeropage = operand();
  idleZeroPage(zeropage);
  zeropage += Y;
  return zeropage;
}
