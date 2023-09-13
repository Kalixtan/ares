struct PV1000 : System {
  auto name() -> string override { return "PV-1000"; }
  auto load(string location) -> bool override;
  auto save(string location) -> bool override;
};

auto PV1000::load(string location) -> bool {

  this->location = locate();
  pak = new vfs::directory;

  return true;
}

auto PV1000::save(string location) -> bool {
  return true;
}
