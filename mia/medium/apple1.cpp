
struct Apple1 : Cartridge {
  auto name() -> string override { return "Apple 1"; }
  auto extensions() -> vector<string> override { return {"pv1"}; }
  auto load(string location) -> bool override;
  auto save(string location) -> bool override;
  auto analyze(vector<u8>& rom) -> string;
};

auto Apple1::load(string location) -> bool {
  vector<u8> rom;
  if(directory::exists(location)) {
    append(rom, {location, "program.rom"});
  } else if(file::exists(location)) {
    rom = Cartridge::read(location);
  }
  if(!rom) return false;

  this->location = location;
  this->manifest = analyze(rom);
  auto document = BML::unserialize(manifest);
  if(!document) return false;

  pak = new vfs::directory;
  pak->setAttribute("title",  document["game/title"].string());
  pak->append("manifest.bml", manifest);
  pak->append("program.rom",  rom);

  return true;
}

auto Apple1::save(string location) -> bool {
  auto document = BML::unserialize(manifest);

  return true;
}

auto Apple1::analyze(vector<u8>& rom) -> string {
  string s;
  s += "game\n";
  s +={"  name:  ", Medium::name(location), "\n"};
  s +={"  title: ", Medium::name(location), "\n"};
  s += "  board\n";
  s += "    memory\n";
  s += "      type: ROM\n";
  s +={"      size: 0x", hex(rom.size()), "\n"};
  s += "      content: Program\n";
  return s;
}
