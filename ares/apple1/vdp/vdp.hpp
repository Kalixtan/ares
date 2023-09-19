struct VDP : Thread {
  Node::Object node;
  Node::Video::Screen screen;

  //vdp.cpp
  auto load(Node::Object) -> void;
  auto unload() -> void;

  auto main() -> void;
  auto main_sound() -> void;
  auto step(u32 clocks) -> void;
  auto frame() -> void;
  auto power() -> void;
  
  auto read(n16 adresss) -> u8;
  auto write(u8 data) -> void;

  n16 hcounter;
  n16 vcounter;
  
  n8 cursx;
  n8 cursy;
  
  //color.cpp
  auto color(n32) -> n64;

  //serialization.cpp
  auto serialize(serializer&) -> void;
};

extern VDP vdp;
