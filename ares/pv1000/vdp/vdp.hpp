struct VDP : Thread {
  Node::Object node;
  Node::Video::Screen screen;

  //vdp.cpp
  auto load(Node::Object) -> void;
  auto unload() -> void;

  auto main() -> void;
  auto step(u32 clocks) -> void;
  auto irq(bool line) -> void;
  auto frame() -> void;
  auto power() -> void;

  n16 hcounter;
  n16 vcounter;
  
  n8 pcg_bank;
  n8 border_col; // Currently unused
  bool force_pattern;

  //color.cpp
  auto color(n32) -> n64;

  //serialization.cpp
  auto serialize(serializer&) -> void;
};

extern VDP vdp;
