struct MegaDuck : GameBoy {
  auto name() -> string override { return "MegaDuck"; }
  auto extensions() -> vector<string> override { return {"gb", "gbc"}; }
};
