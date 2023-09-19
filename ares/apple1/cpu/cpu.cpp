#include <apple1/apple1.hpp>

namespace ares::Apple1 {

CPU cpu;
#include "memory.cpp"
#include "timing.cpp"
#include "debugger.cpp"
#include "serialization.cpp"

auto CPU::load(Node::Object parent) -> void {
  ram.allocate(0x1000);
  node = parent->append<Node::Object>("CPU");
  debugger.load(node);
}

auto CPU::unload() -> void {
  ram.reset();
  debugger = {};
  node = {};
}

auto CPU::main() -> void {
  debugger.instruction();
  instruction();
}

auto CPU::step(u32 clocks) -> void {
  if(io.rdyLine == 1) io.scanlineCycles += clocks;
  Thread::step(clocks);
  Thread::synchronize();
}

auto CPU::power(bool reset) -> void {
  MOS6502::BCD = 1;
  MOS6502::power();
  Thread::create(system.frequency() / 3, {&CPU::main, this});

  PC.byte(0) = readBus(0xfffc);
  PC.byte(1) = readBus(0xfffd);

  io = {};
  ram.fill(0);
  
  dspready = true;
}

}
