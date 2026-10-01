auto WDC65C02S::instructionNone(addr mode, algorithm alg) -> void {
  MAR = ADDR();
  ALG();
}

auto WDC65C02S::instructionLoad(addr mode, algorithm alg) -> void {
  MAR = ADDR();
L MDR = read(MAR);
  ALG();
}

auto WDC65C02S::instructionStore(addr mode, algorithm alg) -> void {
  MAR = ADDR();
  ALG();
L write(MAR, MDR);
}

//NMOS RMW spuriously writes the old value back before writing the new one;
//65C02 fixes that glitch by re-reading the old value instead, so the total
//cycle count is unchanged, just read/read/write instead of read/write/write.
auto WDC65C02S::instructionModify(addr mode, algorithm alg) -> void {
  MAR = ADDR();
  MDR = read(MAR);
  read(MAR);
  ALG();
L write(MAR, MDR);
}

//BBR/BBS: read a zero-page byte, test one bit of it, then run the same
//relative-branch machinery every other conditional branch uses.
auto WDC65C02S::instructionBitBranch(n3 bit, n1 set) -> void {
  n8 zp = operand();
  n8 data = load(zp);
  load(zp);  //RMW-style dummy re-read of the same byte, like instructionModify
  bool take = set ? data.bit(bit) : !data.bit(bit);
  MAR = addressRelative();
  algorithmBranch(take, true);
}
