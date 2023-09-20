auto CPU::read(n16 address) -> n8 {
  // bios
  if (address < 0x4000) return system.bios[address];
  // cart
  //if (address < 0x4000+0x8000) return cartridge.read(address-0x4000);
  if (address < 0xC000){
    //print("Cart-Read: ");
    //print(address);
    //print("\n");
	  return 0;
  }
  // ram
  if (address >= 0xE000 && address < 0xe800) return ram.read(address-0xe000);
  
  // VIDEO
  //if (address == 0xe000) return vdp.data();
  //if (address == 0xe002) return vdp.status();

  return 0;
}

auto CPU::write(n16 address, n8 data) -> void {
  // ram
  if (address >= 0xE000 && address < 0xe800) return ram.write(address-0xe000,data);
  /*
    print("unknown-write: ");
    print(address);
    print("\t");
    print(data);
    print("\n");//*/
  return;
}

auto CPU::out(n16 address, n8 data) -> void {
  address &= 0xff;
    
  switch(address) {
  case 0x80: // TMS9918 write vram
    print("TMS-DWRITE: ");
    print(data);
    print("\n");
    
    vdp.data(data);
    return;
  case 0x81: // TMS9918 write register
    print("TMS-CWRITE: ");
    print(data);
    print("\n");
    vdp.control(data);
    return;
  }
  if ( address <= 0x7f ){
	  m_input_select = data;
	  return;
  }
  //*
    print("io-WRITE: ");
    print(address);
    print("\t");
    print(data);
    print("\n");
  //*/
}

auto CPU::in(n16 address) -> n8 {
  address &= 0xff;
  switch(address) {
  case 0x80: // TMS9918 read vram
    print("TMS9918-VREAD\n");
    return vdp.data();
  case 0x81: // TMS9918 read register
    print("TMS9918-RREAD\n");
    return vdp.status();
  }
  if ( address <= 0x7f ){
	  switch ((m_input_select>>4)&3){
		case 0b10:
		  print("BUTTON 0\n");
		  return 0;//this.button[0]
		case 0b01:
		  print("BUTTON 1\n");
		  return 0;//this.button[1]
		case 0b11:
		  print("BUTTON 2\n");
		  return 0;//this.button[2]
	  }
	  return 0;
  }

  //*
    print("io-READ: ");
    print(address);
    print("\n");
  //*/
  
  return 0xff;
}

