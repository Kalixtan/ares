struct VDP : Thread {
  Node::Object node;
  Node::Video::Screen screen;
  Node::Audio::Stream stream;

  //vdp.cpp
  auto load(Node::Object) -> void;
  auto unload() -> void;

  auto main() -> void;
  auto main_sound() -> void;
  auto step(u32 clocks) -> void;
  auto frame() -> void;
  auto power() -> void;
  
  auto in(n16 adresss) -> u8;
  auto out(n16 adresss, u8 data) -> void;

  n16 hcounter;
  n16 vcounter;
  
  n8 patten_bank;
  n8 tile_bank;
  n8 tile_bank_ram;
  n8 fd_data;
  n8 buffer_flag;
  
  n8 border_col; // Currently unused
  bool pattern_ram_disable;
  bool display_disable;
  
  //sound
  bool square_1;
  bool square_2;
  bool square_3;
  
  n8 square_p1;
  n8 square_p2;
  n8 square_p3;
  

  //color.cpp
  auto color(n32) -> n64;

  //serialization.cpp
  auto serialize(serializer&) -> void;
};

extern VDP vdp;
