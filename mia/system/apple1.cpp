struct Apple1 : System {
  auto name() -> string override { return "Apple 1"; }
  auto load(string location) -> bool override;
  auto save(string location) -> bool override;
};

auto Apple1::load(string location) -> bool {
  auto bios = Pak::read(location);
  if(!bios) return false;

  this->location = locate();
  pak = new vfs::directory;
  pak->append("bios.rom", bios);
  return true;
}

auto Apple1::save(string location) -> bool {
  return true;
}
