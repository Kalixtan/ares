#pragma once
//started: 2023-07-19

#include <ares/ares.hpp>

#include <component/processor/z80/z80.hpp>
//#include <component/video/tms9918/tms9918.hpp>

namespace ares::PV1000 {
  #include <ares/inline.hpp>
  auto enumerate() -> vector<string>;
  auto load(Node::System& node, string name) -> bool;

  #include <pv1000/cpu/cpu.hpp>
  #include <pv1000/vdp/vdp.hpp>

  #include <pv1000/system/system.hpp>
  #include <pv1000/cartridge/cartridge.hpp>
}
