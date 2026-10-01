//Famicom Network System "tsuushin" game cards (JRA-PAT, Nomura no Famicom Trade, Super Mario
//Club): flat, unheadered PRG dumps loaded through the Network Card Slot port, not the main
//cartridge slot -- see reference/FCNS.md and ares/fc/fcns/fcns.hpp. No database entries yet:
//every known dump is 128KB and MMC1-shaped (confirmed for JRA-PAT by the nesdev teardown), so
//for now the whole file is just wrapped as program.rom unconditionally.
struct FamicomNetworkCard : Cartridge {
  auto name() -> string override { return "Famicom Network Card"; }
  auto extensions() -> std::vector<string> override { return {"fcn"}; }
  auto load(string location) -> LoadResult override;
  auto save(string location) -> bool override;
};

auto FamicomNetworkCard::load(string location) -> LoadResult {
  std::vector<u8> rom;
  if(file::exists(location)) {
    rom = Cartridge::read(location);
  }
  if(rom.empty()) return romNotFound;

  this->location = location;

  string s;
  s += "game\n";
  s +={"  name:   ", Medium::name(location), "\n"};
  s +={"  title:  ", Medium::name(location), "\n"};
  s += "  region: NTSC-J\n";
  s += "  board:  nintendo/fcns-card\n";
  s += "    memory\n";
  s +={"      type: ROM\n"};
  s +={"      size: 0x", hex(rom.size()), "\n"};
  s += "      content: Program\n";
  //The cards carry battery-backed SRAM: Super Mario Club's own registration screens ask for a
  //membership number, a DDX phone number and a password, and those have to survive a power cycle.
  //The size is unconfirmed -- the real part is probably smaller than the full $6000-$7fff window --
  //so the whole window is claimed, which is a superset and costs nothing.
  s += "    memory\n";
  s += "      type: RAM\n";
  s += "      size: 0x2000\n";
  s += "      content: Save\n";
  //JRA-PAT carries a BR93C46 serial EEPROM (1kbit = 64 x 16-bit = 128 bytes), a separate non-volatile
  //chip from the battery SRAM, holding the DDX server number and login. Declared for every FNS card --
  //SMC/Nomura simply never touch it, so their .eeprom stays blank. Its own file, per the real hardware.
  s += "    memory\n";
  s += "      type: EEPROM\n";
  s += "      size: 0x80\n";
  s += "      content: Save\n";
  this->manifest = s;

  pak = std::make_shared<vfs::directory>();
  pak->setAttribute("title", Medium::name(location));
  pak->append("manifest.bml", manifest);
  pak->append("program.rom", rom);

  auto document = BML::unserialize(manifest);
  if(auto node = document["game/board/memory(type=RAM,content=Save)"]) {
    Medium::load(node, ".ram");
  }
  if(auto node = document["game/board/memory(type=EEPROM,content=Save)"]) {
    Medium::load(node, ".eeprom");
  }

  return successful;
}

auto FamicomNetworkCard::save(string location) -> bool {
  auto document = BML::unserialize(manifest);

  if(auto node = document["game/board/memory(type=RAM,content=Save)"]) {
    Medium::save(node, ".ram");
  }
  if(auto node = document["game/board/memory(type=EEPROM,content=Save)"]) {
    Medium::save(node, ".eeprom");
  }

  return true;
}
