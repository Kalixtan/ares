extern Random random;

struct System {
  Node::System node;
  VFS::Pak pak;

  struct Controls {
    Node::Object node;
    Node::Input::Button reset;
    Node::Input::Button select;
    Node::Input::Button leftDifficulty;
    Node::Input::Button rightDifficulty;
    Node::Input::Button tvType;

    //controls.cpp
    auto load(Node::Object) -> void;
    auto poll() -> void;
  } controls;

  auto name() const -> string { return information.name; }
  auto frequency() const -> f64 { return information.frequency; }

  //system.cpp
  auto game() -> string;
  auto run() -> void;

  auto load(Node::System& node, string name) -> bool;
  auto save() -> void;
  auto unload() -> void;
  auto power(bool reset) -> void;

  //serialization.cpp
  auto serialize(bool synchronize) -> serializer;
  auto unserialize(serializer&) -> bool;

  u8 bios[0x2000];
  
private:
  struct Information {
    string name = "Apple 1";
    f64 frequency = 3579546;
  } information;

  //serialization.cpp
  auto serialize(serializer&, bool synchronize) -> void;
};

extern System system;
