struct BBC_Bridge_Companion : System {
  auto name() -> string override { return "BBC Bridge Companion"; }
  auto load(string location) -> bool override;
  auto save(string location) -> bool override;
};

auto BBC_Bridge_Companion::load(string location) -> bool {
  auto bios = Pak::read(location);
  if(!bios) return false;

  this->location = locate();
  pak = new vfs::directory;
  pak->append("bios.rom", bios);
  return true;
}

auto BBC_Bridge_Companion::save(string location) -> bool {
  return true;
}
