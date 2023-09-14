#pragma once
//started: 2023-07-19

#include <ares/ares.hpp>

#include <component/processor/z80/z80.hpp>
#include <component/video/tms9918/tms9918.hpp>
#include <component/audio/ay38910/ay38910.hpp>

namespace ares::BBC_Bridge_Companion {
  #include <ares/inline.hpp>
  auto enumerate() -> vector<string>;
  auto load(Node::System& node, string name) -> bool;

  #include <bbcbc/cpu/cpu.hpp>
  #include <bbcbc/vdp/vdp.hpp>
  #include <bbcbc/psg/psg.hpp>

  #include <bbcbc/system/system.hpp>
  #include <bbcbc/cartridge/cartridge.hpp>
}
