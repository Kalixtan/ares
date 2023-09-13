struct System {
  Node::System node;
  VFS::Pak pak;

  struct Controls {
    Node::Object node;
    Node::Input::Button Select;
    Node::Input::Button Start;
    
    Node::Input::Button A;
    Node::Input::Button B;
    
    Node::Input::Button Up;
    Node::Input::Button Down;
    Node::Input::Button Left;
    Node::Input::Button Right;

    //controls.cpp
    auto load(Node::Object) -> void;
    auto poll() -> void;
  } controls;

  auto name() const -> string { return information.name; }
  auto frequency() const -> f64 { return 17897725; }
  
  //system.cpp
  auto game() -> string;
  auto run() -> void;

  auto load(Node::System& node, string name) -> bool;
  auto save() -> void;
  auto unload() -> void;
  auto power(bool reset = false) -> void;

  //serialization.cpp
  auto serialize(bool synchronize) -> serializer;
  auto unserialize(serializer&) -> bool;

private:
  struct Information {
    string name = "PV-1000";
  } information;

  //serialization.cpp
  auto serialize(serializer&, bool synchronize) -> void;
};

extern System system;
