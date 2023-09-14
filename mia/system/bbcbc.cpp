struct BBC_Bridge_Companion : System {
  auto name() -> string override { return "BBC Bridge Companion"; }
  auto load(string location) -> bool override;
  auto save(string location) -> bool override;
};

auto BBC_Bridge_Companion::load(string location) -> bool {

  this->location = locate();
  pak = new vfs::directory;

  return true;
}

auto BBC_Bridge_Companion::save(string location) -> bool {
  return true;
}
