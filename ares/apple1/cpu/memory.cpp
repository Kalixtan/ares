inline auto CPU::readBus(n16 address) -> n8 {
	
  if (address < 0xbfff) return ram.read(address & 0xfff); // RAM
  
  //if (address >= 0xd010 && address <= 0xd013){ // PIA-6821
      switch(address) {
      case 0xd012:  // dsp ( Display ready? )
        //print("dsp\n");
        
        if (dspready == true){
          return 0x80;
	    }
        return 0x00;
        
      case 0xd013:  // dspcr (UNUSED)
        print("dspcr\n");
        return 0x80;
        
      case 0xd010:  // kbd
        print("kbd\n");
        return 0x80;
        
      case 0xd011:  // kbdcr ( Keyboard ready? )
        print("kbdcr\n");
        /*
        if self.terminal.kbdready == True:
          return 0x80
        else:
          return 0x00
        */
        return 0x80;
	  }
  //}
  
  if (address >= 0xe000 && address < 0xefff) return ram.read(address & 0xfff); // RAM
  

  if (address >= 0xff00) return system.bios[address-0xff00];
  
  print("NO_HIT_READ:\tA: ");
  print(address);
  print("\n");
  return 0;
}

inline auto CPU::writeBus(n16 address, n8 data) -> void {
  if (address < 0xbfff) return ram.write(address & 0xfff, data); // RAM
  
  char aChar = data & 0x7f;
        
  //if (address >= 0xd010 && address <= 0xd013){ // PIA-6821
      switch(address) {
      case 0xd012:  // dsp
        vdp.write(data);
        return;
        
      case 0xd013:  // dspcr (UNUSED)
        //print("dspcr:\t");
        //print(data);
        //print("\n");
        return;
        
      case 0xd010:  // kbd
        print("kbd:\t");
        print(data);
        print("\n");
        return;
        
      case 0xd011:  // kbdcr (UNUSED)
        //print("kbdcr:\t");
        //print(data);
        //print("\n");
        return;
	  }
  //}
  
  if (address >= 0xe000 && address < 0xefff) return ram.write(address & 0xfff, data); // RAM
  
  print("NO_HIT_WRITE:\tA: ");
  print(address);
  print("\tD: ");
  print(data);
  print("\n");
}

auto CPU::readDebugger(n16 address) -> n8 {
  return readBus(address);
}
