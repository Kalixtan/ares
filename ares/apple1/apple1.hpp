#pragma once

#include <ares/ares.hpp>

#include <component/processor/mos6502/mos6502.hpp>

namespace ares::Apple1 {
  #include <ares/inline.hpp>
  auto enumerate() -> vector<string>;
  auto load(Node::System& node, string name) -> bool;

  #include <apple1/controller/controller.hpp>
  #include <apple1/system/system.hpp>
  #include <apple1/cartridge/cartridge.hpp>
  #include <apple1/cpu/cpu.hpp>
  #include <apple1/vdp/vdp.hpp>
}

