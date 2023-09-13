auto VDP::color(n32 color) -> n64 {
	/*
0x000000,
0x0000ff,
0x00ff00,
0x00ffff,
0xff0000,
0xff00ff,
0xffff00,
0xffffff,
	 */
  switch(color.bit(0,3)) {
  case  0: return 0x0000'0000'0000ull;  //
  case  1: return 0xffff'0000'0000ull;  //
  case  2: return 0x0000'ffff'0000ull;  //
  case  3: return 0xffff'ffff'0000ull;  //
  case  4: return 0x0000'0000'ffffull;  //
  case  5: return 0xffff'0000'ffffull;  //
  case  6: return 0x0000'ffff'ffffull;  //
  case  7: return 0xffff'ffff'ffffull;  //
  }
  unreachable;
}
