auto CPU::Debugger::load(Node::Object parent) -> void {
  memory.ram = parent->append<Node::Debugger::Memory>("CPU RAM");
  memory.ram->setSize(cpu.ram.size());
  memory.ram->setRead([&](u32 address) -> u8 {
    return cpu.ram[address];
  });
  memory.ram->setWrite([&](u32 address, u8 data) -> void {
    cpu.ram[address] = data;
  });

  tracer.instruction = parent->append<Node::Debugger::Tracer::Instruction>("Instruction", "CPU");
  tracer.instruction->setAddressBits(16);
}

auto CPU::Debugger::instruction() -> void {
  if(tracer.instruction->enabled() && tracer.instruction->address(cpu.PC)) {
    tracer.instruction->notify(cpu.disassembleInstruction(), cpu.disassembleContext());
  }
}


