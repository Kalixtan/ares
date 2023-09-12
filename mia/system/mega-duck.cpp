struct MegaDuck : System {
  auto name() -> string override { return "MegaDuck"; }
  auto load(string location) -> bool override;
  auto save(string location) -> bool override;
};

auto MegaDuck::load(string location) -> bool {
  this->location = locate();
  pak = new vfs::directory;
  //pak->append("boot.rom", Resource::GameBoy::BootDMG1);
  return true;
}

auto MegaDuck::save(string location) -> bool {
  return true;
}
