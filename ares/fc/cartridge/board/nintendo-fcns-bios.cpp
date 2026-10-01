//Famicom Network System adapter: the RF5C66/RF5A18 unit's own fixed ROMs (RF5A18's 8KB internal
//program ROM, and the 256KB LH5323M1 Kanji font ROM), loaded the same way HVC_FMR loads the FDS
//BIOS -- as "the cartridge" for this System::Model, with the actual swappable game (JRA-PAT,
//Nomura no Famicom Trade, Super Mario Club) inserted afterward through its own hot-swappable
//port (fcnsCard, ares/fc/fcns/fcns.hpp) the same way FDS disks are. See reference/FCNS.md.
struct NintendoFCNSBios : Interface {
  static auto create(string id) -> Interface* {
    if(id == "nintendo/fcns-bios") return new NintendoFCNSBios;
    return nullptr;
  }

  auto load() -> void override {
    fcns.present = 1;
    Interface::load(fcns.cpu2.rom, "program.rom");
    Interface::load(fcns.kanjiROM, "option.rom");
    fcns.load();
  }

  //the RF5C66's M2 cycle counter has to advance once per CPU cycle, the same way HVC_FMR drives
  //the FDS timer from here.
  auto main() -> void override {
    fcns.main();
    tick();
  }

  auto readPRG(n32 address, n8 data) -> n8 override {
    return fcns.read(address, data);
  }

  auto writePRG(n32 address, n8 data) -> void override {
    fcns.write(address, data);
  }

  //the card has no PPU/CHR connection at all (confirmed on nesdev) -- pattern-table fetches come
  //from the adapter's own onboard CHR-RAM. That RAM is not in the CPU address space either: the
  //RF5C66 cannot write it, so games fill it the ordinary NES way, via $2006/$2007 -> writeCHR().
  //$40ad bit 7 picks which PPU address line drives CIRAM A10: 0 = vertical (PPU A10), 1 =
  //horizontal (PPU A11). Previously hardcoded vertical.
  auto addressCIRAM(n32 address) const -> n32 {
    return address >> fcns.mirror & 0x0400 | address & 0x03ff;
  }

  auto readCHR(n32 address, n8 data) -> n8 override {
    if(address & 0x2000) return ppu.readCIRAM(addressCIRAM(address));
    return fcns.characterRAM[fcns.chrBank].read(address);
  }

  auto writeCHR(n32 address, n8 data) -> void override {
    if(address & 0x2000) return ppu.writeCIRAM(addressCIRAM(address), data);
    fcns.characterRAM[fcns.chrBank].write(address, data);
  }

  auto power() -> void override {
    fcns.power();
  }

  auto serialize(serializer& s) -> void override {
    s(fcns);
  }
};
