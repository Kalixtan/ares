auto WDC65C02S::algorithmADC() -> void {
  n8 i = MDR;
  i16 o = A + i + C;
  if(!BCD || !D) {
    C = o.bit(8);
    Z = n8(o) == 0;
    N = o.bit(7);
    V = ~(A ^ i) & (A ^ o) & 0x80;
    A = o;
    return;
  }

  //65C02 decimal ADC: like NMOS, V is derived from the pre-adjustment
  //nibble math and isn't guaranteed meaningful in decimal mode. unlike
  //NMOS, N and Z are taken from the final BCD-adjusted result rather than
  //the raw binary sum, matching real 65C02 hardware. one extra cycle, same
  //as NMOS, for the digit correction: it re-reads the operand's effective
  //address (confirmed against SingleStepTests' wdc65c02 corpus) rather
  //than idling on PC; immediate addressing has no real effective address
  //to re-read, so its exact bus address here is an unresolved hardware
  //quirk this doesn't reproduce (harmless: no side effects on a phantom
  //read, and firmware doesn't do decimal-mode ADC/SBC #imm in practice).
  read(MAR);
  n8 lo = (A & 0x0f) + (i & 0x0f) + (C << 0);
  if(lo > 0x09) lo += 0x06;
  n8 carryLo = lo > 0x0f;
  n16 hi = (A & 0xf0) + (i & 0xf0) + (carryLo << 4) + (lo & 0x0f);
  V = ~(A ^ i) & (A ^ n8(hi)) & 0x80;
  if(hi > 0x9f) hi += 0x60;
  C = hi > 0xff;
  A = hi;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmAND() -> void {
  A &= MDR;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmASLA() -> void {
  C = A.bit(7);
  A <<= 1;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmASLM() -> void {
  C = MDR.bit(7);
  MDR <<= 1;
  Z = MDR == 0;
  N = MDR.bit(7);
}

auto WDC65C02S::algorithmBCC() -> void {
  algorithmBranch(C == 0);
}

auto WDC65C02S::algorithmBCS() -> void {
  algorithmBranch(C == 1);
}

auto WDC65C02S::algorithmBEQ() -> void {
  algorithmBranch(Z == 1);
}

auto WDC65C02S::algorithmBIT() -> void {
  Z = (A & MDR) == 0;
  V = MDR.bit(6);
  N = MDR.bit(7);
}

//65C02 adds BIT #immediate. unlike the zp/abs forms, an immediate operand
//has no meaningful bit 6/7 of "memory" to report, so only Z is affected;
//N and V are left untouched.
auto WDC65C02S::algorithmBITImmediate() -> void {
  Z = (A & MDR) == 0;
}

auto WDC65C02S::algorithmBMI() -> void {
  algorithmBranch(N == 1);
}

auto WDC65C02S::algorithmBNE() -> void {
  algorithmBranch(Z == 0);
}

auto WDC65C02S::algorithmBPL() -> void {
  algorithmBranch(N == 0);
}

//new, unconditional relative branch.
auto WDC65C02S::algorithmBRA() -> void {
  algorithmBranch(true);
}

auto WDC65C02S::algorithmBRK() -> void {
  MDR = read(MAR); // dummy implied read
  PC++;
  push(PCH);
  push(PCL);

  n16 vector = 0xfffe;
  nmi(vector);

  push(P | 0x30);
  I = 1;
  //65C02 hardware clears D after pushing P on BRK/IRQ/NMI, so a decimal
  //interrupt handler doesn't inherit decimal mode by accident. NMOS does
  //not do this. MAME's w65c02 core does not model it either (checked
  //against w65c02.cpp/m6502.cpp — neither touches F_D here), but it is
  //documented WDC hardware behaviour, not a guess; flagged for
  //confirmation once real Supervision software exercises an interrupt.
  D = 0;

  PCL = read(vector++);
  cancelNmi();
  PCH = read(vector++);
}

//RMB0-7/SMB0-7: clear or set a single bit of the zero-page byte just read
//into MDR; instructionModify writes it back. eight near-identical bodies
//rather than one parameterized over a runtime bit number, matching how the
//existing table dispatches algorithms with no arguments.
auto WDC65C02S::algorithmRMB0() -> void { MDR = MDR & ~0x01; }
auto WDC65C02S::algorithmRMB1() -> void { MDR = MDR & ~0x02; }
auto WDC65C02S::algorithmRMB2() -> void { MDR = MDR & ~0x04; }
auto WDC65C02S::algorithmRMB3() -> void { MDR = MDR & ~0x08; }
auto WDC65C02S::algorithmRMB4() -> void { MDR = MDR & ~0x10; }
auto WDC65C02S::algorithmRMB5() -> void { MDR = MDR & ~0x20; }
auto WDC65C02S::algorithmRMB6() -> void { MDR = MDR & ~0x40; }
auto WDC65C02S::algorithmRMB7() -> void { MDR = MDR & ~0x80; }
auto WDC65C02S::algorithmSMB0() -> void { MDR = MDR | 0x01; }
auto WDC65C02S::algorithmSMB1() -> void { MDR = MDR | 0x02; }
auto WDC65C02S::algorithmSMB2() -> void { MDR = MDR | 0x04; }
auto WDC65C02S::algorithmSMB3() -> void { MDR = MDR | 0x08; }
auto WDC65C02S::algorithmSMB4() -> void { MDR = MDR | 0x10; }
auto WDC65C02S::algorithmSMB5() -> void { MDR = MDR | 0x20; }
auto WDC65C02S::algorithmSMB6() -> void { MDR = MDR | 0x40; }
auto WDC65C02S::algorithmSMB7() -> void { MDR = MDR | 0x80; }

//stops fetching until an interrupt is pending; the consuming CPU class's
//dispatch loop must check `waiting` before calling instruction() each cycle
//and idle instead. real hardware wakes on IRQ or NMI regardless of the I
//flag, but only actually services the interrupt if it's unmasked — servicing
//itself is unaffected here since that's already how interrupt() works.
auto WDC65C02S::algorithmWAI() -> void {
  waiting = 1;
}

//stops the processor until a hardware reset; nothing but reset clears this.
auto WDC65C02S::algorithmSTP() -> void {
  stopped = 1;
}

auto WDC65C02S::algorithmBVC() -> void {
  algorithmBranch(V == 0);
}

auto WDC65C02S::algorithmBVS() -> void {
  algorithmBranch(V == 1);
}

//bitBranch selects BBR/BBS's page-crossing behavior instead of a regular
//conditional branch's: confirmed against SingleStepTests' wdc65c02 corpus,
//BBR/BBS re-read the not-yet-updated PC twice on a page-crossing branch,
//while plain Bcc keeps the NMOS invalid-(old-page,new-low)-address glitch.
auto WDC65C02S::algorithmBranch(bool take, bool bitBranch) -> void {
  if(!take) {
  L MDR = read(MAR); // dummy relative read
  } else {
    bool irq = irqPending();
    MDR = read(MAR);
    n16 newPC = PC + (i8)MDR;
    bool pageCrossed = newPC >> 8 != PC >> 8;

    if (pageCrossed) {
      idle();
      if(bitBranch) {
      L idle();
      } else {
      L read(PC & 0xff00 | newPC & 0x00ff);
      }
    } else {
      // a taken non-page-crossing branch ignores
      // IRQ/NMI during its last clock, so that
      // next instruction executes before the IRQ
      if (!irq)
        delayIrq();
      else
        lastCycle();

    L idle();
    }
    PC = newPC;
  }
}

auto WDC65C02S::algorithmCLC() -> void {
  C = 0;
}

auto WDC65C02S::algorithmCLD() -> void {
  D = 0;
}

auto WDC65C02S::algorithmCLI() -> void {
  I = 0;
}

auto WDC65C02S::algorithmCLV() -> void {
  V = 0;
}

auto WDC65C02S::algorithmCMP() -> void {
  n9 o = A - MDR;
  C = !o.bit(8);
  Z = n8(o) == 0;
  N = o.bit(7);
}

auto WDC65C02S::algorithmCPX() -> void {
  n9 o = X - MDR;
  C = !o.bit(8);
  Z = n8(o) == 0;
  N = o.bit(7);
}

auto WDC65C02S::algorithmCPY() -> void {
  n9 o = Y - MDR;
  C = !o.bit(8);
  Z = n8(o) == 0;
  N = o.bit(7);
}

auto WDC65C02S::algorithmDEC() -> void {
  MDR--;
  Z = MDR == 0;
  N = MDR.bit(7);
}

//new: DEC A. same math as DEC on memory, operating on the accumulator via
//implied addressing (no memory cycle at all, unlike DEC on a memory operand).
auto WDC65C02S::algorithmDECA() -> void {
  A--;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmDEX() -> void {
  X--;
  Z = X == 0;
  N = X.bit(7);
}

auto WDC65C02S::algorithmDEY() -> void {
  Y--;
  Z = Y == 0;
  N = Y.bit(7);
}

auto WDC65C02S::algorithmEOR() -> void {
  A ^= MDR;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmINC() -> void {
  MDR++;
  Z = MDR == 0;
  N = MDR.bit(7);
}

//new: INC A, the accumulator counterpart to DEC A above.
auto WDC65C02S::algorithmINCA() -> void {
  A++;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmINX() -> void {
  X++;
  Z = X == 0;
  N = X.bit(7);
}

auto WDC65C02S::algorithmINY() -> void {
  Y++;
  Z = Y == 0;
  N = Y.bit(7);
}

auto WDC65C02S::algorithmJMP() -> void {
  PC = MAR;
}

auto WDC65C02S::algorithmJSR() -> void {
  n16 target = operand();
  MDR = read(0x0100 | S); // dummy stack read
  push(PCH);
  push(PCL);
L target |= operand() << 8;
  PC = target;
}

auto WDC65C02S::algorithmLDA() -> void {
  A = MDR;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmLDX() -> void {
  X = MDR;
  Z = X == 0;
  N = X.bit(7);
}

auto WDC65C02S::algorithmLDY() -> void {
  Y = MDR;
  Z = Y == 0;
  N = Y.bit(7);
}

auto WDC65C02S::algorithmLSRA() -> void {
  C = A.bit(0);
  A >>= 1;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmLSRM() -> void {
  C = MDR.bit(0);
  MDR >>= 1;
  Z = MDR == 0;
  N = MDR.bit(7);
}

auto WDC65C02S::algorithmNOP() -> void {}

auto WDC65C02S::algorithmORA() -> void {
  A |= MDR;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmPHA() -> void {
  read(MAR); // dummy implied read
L push(A);
}

auto WDC65C02S::algorithmPHP() -> void {
  MDR = read(MAR); // dummy implied read
L push(P | 0x30);
}

//new: PHX/PHY, PLX/PLY. same shape as PHA/PLA, on X and Y instead of A.
auto WDC65C02S::algorithmPHX() -> void {
  read(MAR); // dummy implied read
L push(X);
}

auto WDC65C02S::algorithmPHY() -> void {
  read(MAR); // dummy implied read
L push(Y);
}

auto WDC65C02S::algorithmPLA() -> void {
  MDR = read(MAR);        // dummy implied read
  MDR = read(0x0100 | S); // dummy stack read
L A = pull();
  Z = n8(A) == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmPLP() -> void {
  MDR = read(MAR);        // dummy implied read
  MDR = read(0x0100 | S); // dummy stack read
L P = pull();
  //bits 4/5 have no physical storage: only PHP/BRK's push synthesizes them,
  //so PLP discards whatever was pulled into their positions (confirmed
  //against SingleStepTests' wdc65c02 corpus).
  P.b = 0;
  P.u = 1;
}

auto WDC65C02S::algorithmPLX() -> void {
  MDR = read(MAR);        // dummy implied read
  MDR = read(0x0100 | S); // dummy stack read
L X = pull();
  Z = X == 0;
  N = X.bit(7);
}

auto WDC65C02S::algorithmPLY() -> void {
  MDR = read(MAR);        // dummy implied read
  MDR = read(0x0100 | S); // dummy stack read
L Y = pull();
  Z = Y == 0;
  N = Y.bit(7);
}

auto WDC65C02S::algorithmROLA() -> void {
  bool c = C;
  C = A.bit(7);
  A = A << 1 | c;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmROLM() -> void {
  bool c = C;
  C = MDR.bit(7);
  MDR = MDR << 1 | c;
  Z = MDR == 0;
  N = MDR.bit(7);
}

auto WDC65C02S::algorithmRORA() -> void {
  bool c = C;
  C = A.bit(0);
  A = A >> 1 | c << 7;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmRORM() -> void {
  bool c = C;
  C = MDR.bit(0);
  MDR = MDR >> 1 | c << 7;
  Z = MDR == 0;
  N = MDR.bit(7);
}

auto WDC65C02S::algorithmRTI() -> void {
  MDR = read(MAR);        // dummy implied read
  MDR = read(0x0100 | S); // dummy stack read
  P = pull();
  //see algorithmPLP: bits 4/5 have no physical storage, so RTI's pull
  //discards whatever was in their positions too.
  P.b = 0;
  P.u = 1;
  PCL = pull();
L PCH = pull();
}

auto WDC65C02S::algorithmRTS() -> void {
  MDR = read(MAR);        // dummy implied read
  MDR = read(0x0100 | S); // dummy stack read
  PCL = pull();
  PCH = pull();
L idle();
  PC++;
}

auto WDC65C02S::algorithmSBC() -> void {
  n8 i = ~MDR;
  i16 o = A + i + C;
  if(!BCD || !D) {
    C = o.bit(8);
    Z = n8(o) == 0;
    N = o.bit(7);
    V = ~(A ^ i) & (A ^ o) & 0x80;
    A = o;
    return;
  }

  //65C02 decimal SBC: allows interdigit borrow from the decimal adjustment
  //(NMOS does not), and N/Z are taken from the final adjusted result.
  //mirrors MAME's w65c02_device::do_sbc_cd() line for line. the extra
  //cycle re-reads the effective address, same caveat as algorithmADC above.
  read(MAR);
  n8 val = MDR;
  n8 c = C ? 0 : 1;
  n16 diff = A - val - c;
  n8 al = (A & 0x0f) - (val & 0x0f) - c;
  n8 ah = (A >> 4) - (val >> 4) - (al.bit(7) ? 1 : 0);
  V = (A ^ val) & (A ^ n8(diff)) & 0x80;
  C = diff <= 0xff;
  n8 result = (ah << 4) | (al & 0x0f);
  if(al.bit(7)) result -= 0x06;
  if(ah.bit(7)) result -= 0x60;
  A = result;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmSEC() -> void {
  C = 1;
}

auto WDC65C02S::algorithmSED() -> void {
  D = 1;
}

auto WDC65C02S::algorithmSEI() -> void {
  I = 1;
}

auto WDC65C02S::algorithmSTA() -> void {
  MDR = A;
}

auto WDC65C02S::algorithmSTX() -> void {
  MDR = X;
}

auto WDC65C02S::algorithmSTY() -> void {
  MDR = Y;
}

//new: STZ, store zero. no flags affected, same shape as STA/STX/STY.
auto WDC65C02S::algorithmSTZ() -> void {
  MDR = 0;
}

auto WDC65C02S::algorithmTAX() -> void {
  X = A;
  Z = X == 0;
  N = X.bit(7);
}

auto WDC65C02S::algorithmTAY() -> void {
  Y = A;
  Z = Y == 0;
  N = Y.bit(7);
}

//new: TRB, test and reset bits. Z reports the AND test like BIT; the
//accumulator's bits are then cleared out of memory rather than tested only.
auto WDC65C02S::algorithmTRB() -> void {
  Z = (A & MDR) == 0;
  MDR &= ~A;
}

//new: TSB, test and set bits. same Z semantics as TRB, but ORs the
//accumulator's bits into memory instead of clearing them.
auto WDC65C02S::algorithmTSB() -> void {
  Z = (A & MDR) == 0;
  MDR |= A;
}

auto WDC65C02S::algorithmTSX() -> void {
  X = S;
  Z = X == 0;
  N = X.bit(7);
}

auto WDC65C02S::algorithmTXA() -> void {
  A = X;
  Z = A == 0;
  N = A.bit(7);
}

auto WDC65C02S::algorithmTXS() -> void {
  S = X;
}

auto WDC65C02S::algorithmTYA() -> void {
  A = Y;
  Z = A == 0;
  N = A.bit(7);
}
