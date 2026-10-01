ExpansionPort expansionPort{"Expansion Port"};

ExpansionPort::ExpansionPort(string name) : name(name) {
}

auto ExpansionPort::load(Node::Object parent) -> void {
  port = parent->append<Node::Port>(name);
  port->setFamily("Famicom");
  port->setType("Expansion");
  port->setHotSwappable(true);
  port->setAllocate([&](auto name) { return allocate(name); });
  port->setDisconnect([&] { device.reset(); });
  //the Network System's HVC-051 keypad lives on the expansion port (read at $4016 D1), so it
  //leads here on that model; elsewhere the usual expansion devices do.
  if(system.model() == System::Model::FamicomNetworkSystem) {
    port->setSupported({"Network Controller", "EPSM", "Family Keyboard"});
  } else {
    port->setSupported({"EPSM", "Family Keyboard"});
  }
}

auto ExpansionPort::unload() -> void {
  device.reset();
  port.reset();
}

auto ExpansionPort::allocate(string name) -> Node::Peripheral {
  if(name == "EPSM") device = std::make_unique<EPSM>(port);
  if(name == "Family Keyboard") device = std::make_unique<FamilyKeyboard>(port);
  if(name == "Network Controller") device = std::make_unique<NetworkController>(port);
  if(device) return device->node;
  return {};
}

auto ExpansionPort::serialize(serializer& s) -> void {
  if(device) device->serialize(s);
}
