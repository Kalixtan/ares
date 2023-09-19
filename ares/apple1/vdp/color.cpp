auto VDP::color(n32 color) -> n64 {
  switch(color) {
  case  0: return 0x0000'0000'0000ull;  //
  case  1: return 0xffff'ffff'ffffull;  //
  case  2: return 0xffff'0000'0000ull;  // DEBUG
  }
  unreachable;
}
