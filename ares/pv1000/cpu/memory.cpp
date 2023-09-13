auto CPU::read(n16 address) -> n8 {

  // MAIN
  if (address < 0x4000) return cartridge.read(address);
  if (address >= 0xB800 && address <= 0xBFFF) return ram.read(address - 0xB800);
  
  // VIDEO
 // if (address == 0xe000) return vdp.data();
 // if (address == 0xe002) return vdp.status();


  return 0;
}

auto CPU::write(n16 address, n8 data) -> void {
  // MAIN RAM
  if (address >= 0xB800 && address <= 0xBFFF){
	  //print(address);
	  //print("\t");
	  //print(data);
	  //print("\n");
	  ram.write(address - 0xB800, data);
  }
  
  // VIDEO
 // if (address == 0xe000) return vdp.data(data);
 // if (address == 0xe002) return vdp.control(data);


  return;
}

auto CPU::out(n16 address, n8 data) -> void {
  address &= 0b111;
  switch(address) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
      //m_sound->voice_w(offset, data);
      break;

    case 0x05:
      fd_data = 0xf;
      break;
      
    case 0x06: // VRAM + PCG location, always fixed at 0xb8xx??
      //console.log("VRAM + PCG location =",data)
      break;
      
    case 0x07:
      /* ---- -xxx unknown, border color? */
      vdp.pcg_bank = (data & 0x20) >> 5;
      vdp.force_pattern = ((data & 0x10) >> 4); /* Dig Dug relies on this */
      vdp.border_col = data & 7;
      print("bank: ");
      print(vdp.pcg_bank);
      print("\tFP: ");
      print(vdp.force_pattern);
      print("\tBC: ");
      print(vdp.border_col);
      print("\n");
      break;
      
    default:
      //console.log("UNKNOWN",addr,data)
      break;
  }
  
  io[address] = data;
}

auto CPU::controller_read(n16 address) -> n8 {
  n8 input = 0;
  switch(address) {
    case 0x00:
      // player 1
      input.bit(0) = system.controls.Select->value();
      input.bit(1) = system.controls.Start->value();
      
      // player 2
      //input.bit(2) = system.controls.Select->value();
      //input.bit(3) = system.controls.Start->value();
      break;
    case 0x01:
      // player 1
      input.bit(0) = system.controls.Down->value();
      input.bit(1) = system.controls.Right->value();
      
      // player 2
      //input.bit(2) = system.controls.Down->value();
      //input.bit(3) = system.controls.Right->value();
      break;
    case 0x02:
      // player 1
      input.bit(0) = system.controls.Left->value();
      input.bit(1) = system.controls.Up->value();
      
      // player 2
      //input.bit(2) = system.controls.Left->value();
      //input.bit(3) = system.controls.Up->value();
      break;
    case 0x03:
      // player 1
      input.bit(0) = system.controls.A->value();
      input.bit(1) = system.controls.B->value();
      
      // player 2
      //input.bit(2) = system.controls.A->value();
      //input.bit(3) = system.controls.B->value();
      break;
  }
  return input;
}
auto CPU::in(n16 address) -> n8 {
  address &= 0b111;
  
  u8 data = io[address];
  
  switch(address) {
  case 0x04:  //buffer stuff
      // Bit 1 = 1 => Data is available in port FD
      // Bit 0 = 1 => Buffer at port FD is empty
      
      data = 0;
      data = buffer_flag & 1;
      data |= fd_data ? 2 : 0;
      buffer_flag &= ~1;
      /*
      print("fd_data: ");
      print(fd_data);
      print("\tFlag: ");
      print(buffer_flag);
      print("\n");
      //*/
    break;
  case 0x05:  //controls
    
    //static const char *const joynames[] = { "IN0", "IN1", "IN2", "IN3" };
    data = 0;
    for(u8 i : range(4)) {
      if (io[5] & 1 << i){
        data |= controller_read( i ); //this.joy[ i ]
        fd_data &= ~(1 << i);
      }
    }
    /*
    print("joy: ");
    print(data);
    print("\tio: ");
    print(io[5]);
    print("\tfd: ");
    print(fd_data);
    print("\n");
    //*/
    /*
    print("0: ");
    print(controller_read( 0 ));
    print("\t1: ");
    print(controller_read( 1 ));
    print("\t2: ");
    print(controller_read( 2 ));
    print("\t3: ");
    print(controller_read( 3 ));
    print("\t4: ");
    print(controller_read( 4 ));
    print("\n");
    //*/
    //console.log("looking for input got",data)
    //data = Math.floor(Math.random()*256) // random input for testing
    
    break;
  default: // UNKNOWN IO
    break;
  }

  return data;
}









