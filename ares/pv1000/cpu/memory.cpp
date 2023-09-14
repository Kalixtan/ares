auto CPU::read(n16 address) -> n8 {
  // CART
  if (address < 0x8000) return cartridge.read(address);
  
  // RAM
  if (address >= 0xB000 && address <= 0xBFFF) return ram.read((address - 0xB000)%0x800);
  
  return 0;
}

auto CPU::write(n16 address, n8 data) -> void {
  // RAM
  if (address >= 0xB000 && address <= 0xBFFF) return ram.write((address - 0xB000)%0x800,data);
  
  return;
}

auto CPU::out(n16 address, n8 data) -> void {
  if (address >= 0xF8) return vdp.out( address&0xff, data );
  io[address&0b111] = data;
  return;
}

auto CPU::controller_read(n16 address, u8 port) -> n8 {
  n8 input = 0;
  if (port == 0){ // player 1
	  switch(address) { 
		case 0x00:
		  input.bit(0) = system.controls.Select->value();
		  input.bit(1) = system.controls.Start->value();
		  break;
		case 0x01:
		  input.bit(0) = system.controls.Down->value();
		  input.bit(1) = system.controls.Right->value();
		  break;
		case 0x02:
		  input.bit(0) = system.controls.Left->value();
		  input.bit(1) = system.controls.Up->value();
		  break;
		case 0x03:
		  input.bit(0) = system.controls.A->value();
		  input.bit(1) = system.controls.B->value();
		  break;
	  }
  } else { // player 2
	  switch(address) {
		case 0x00:
		  input.bit(2) = system.controls.Select->value();
		  input.bit(3) = system.controls.Start->value();
		  break;
		case 0x01:
		  input.bit(2) = system.controls.Down->value();
		  input.bit(3) = system.controls.Right->value();
		  break;
		case 0x02:
		  input.bit(2) = system.controls.Left->value();
		  input.bit(3) = system.controls.Up->value();
		  break;
		case 0x03:
		  input.bit(2) = system.controls.A->value();
		  input.bit(3) = system.controls.B->value();
		  break;
	  }
  }
  return input;
}
auto CPU::in(n16 address) -> n8 {
  if (address >= 0xF8) return vdp.in( address&0xff );
  return 0;
}









