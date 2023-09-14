#include <pv1000/pv1000.hpp>

namespace ares::PV1000 {

VDP vdp;
#include "color.cpp"
#include "serialization.cpp"

auto VDP::main() -> void {

    const auto pixel = (vcounter * screen->width()) + hcounter;
    const auto tile_index  = ((vcounter>>3) * 32) + (hcounter>>3);
    auto buffer = screen->pixels().data();
    
    // get tile
    auto tile = cpu.ram.read(tile_index + (patten_bank << 8));
    //tile = tile_index; // testing
    auto cart_tile = tile < 0xe0 || pattern_ram_disable;
    if ( cart_tile ){
		tile += tile_bank << 8;
		tile = 0;
	} else {
		tile += tile_bank_ram << 8;
		tile -= 0xe0;//tile &= 0x1f;
	}
	
    // do tile
    const auto offset = (tile*4*8)+8+(vcounter%8);
	auto g1 = cpu.ram.read(offset+0x400   );
	auto g2 = cpu.ram.read(offset+0x400+8 );
	auto g3 = cpu.ram.read(offset+0x400+16);
    
    if ( cart_tile ){
      g1 = cartridge.read(offset   );
      g2 = cartridge.read(offset+8 );
      g3 = cartridge.read(offset+16);
	}
    
    const auto bit = 7-(hcounter%8);
    buffer[pixel] = ((g1>>bit)&1) | (((g2>>bit)&1)<<1) | (((g3>>bit)&1)<<2);
    
  step(1);
  
  //sound (Very broken)
  u16 sound_clock = pixel%8739;
  if (sound_clock%(square_p1) == 0){
    square_1 = !square_1;
  }
  if (sound_clock%(square_p2) == 0){
    square_2 = !square_2;
  }
  if (sound_clock%(square_p3) == 0){
    square_3 = !square_3;
  }
  stream->frame( (square_1*0.5)+(square_2*0.75)+square_3 );
  
  // Increment pixel counters
  hcounter++;
  if (hcounter == 32*8) {
    hcounter = 0;
    vcounter++;
    if (vcounter == 24*8) {
      vcounter = 0;
      
      cpu.setIRQ(true);
      frame();
    }
  }
}

auto VDP::load(Node::Object parent) -> void {
  node = parent->append<Node::Object>("VDP");

  screen = node->append<Node::Video::Screen>("Screen", 32*8, 24*8);
  screen->colors(8, {&VDP::color, this});
  screen->setSize(32*8, 24*8);
  screen->setScale(1.0, 1.0);
  screen->setAspect(1.0, 1.0);
  screen->setViewport(0, 0, screen->width(), screen->height());
  
  
  stream = node->append<Node::Audio::Stream>("Audio");
  stream->setChannels(1);
  stream->setFrequency(system.frequency()/2);
}

auto VDP::unload() -> void {
  screen->quit();
  node->remove(screen);
  node->remove(stream);
  screen.reset();
  node.reset();
}

auto VDP::step(u32 clocks) -> void {
  Thread::step(clocks);
  Thread::synchronize(cpu);
}

auto VDP::in(n16 address) -> u8 {
  u8 data = cpu.io[address];
  
  switch(address) {
  case 0xFC:  //Interrupt status
    /*
	7  bit  0
	ZAAA BBMP
	|||| ||||
	|||| |||+-- 1: prerender IRQ is cause of interrupt, cleared by write to port $FD
	|||| ||+--- 1: matrix scan IRQ is cause of interrupt, cleared by read from port $FD
	|||| ++---- Player 2 joystick inputs, same as reading from $FD
	|+++------- Always 0
	+---------- Always 1 (Probably pulled up by resistor next to Z80)

     */
      // Bit 1 = 1 => Data is available in port FD
      // Bit 0 = 1 => Buffer at port FD is empty
      
      // Is any of this correct (Taken directly from JAMES)
      data = 0;
      data = buffer_flag & 1;
      data |= fd_data ? 2 : 0;
      buffer_flag &= ~1;
    break;
    
  case 0xFD:  //GPO (Also acknowledges Matrix scan IRQ. )
    /*
	7  bit  0
	ZAAA BBCC
	|||| ||||
	|||| ||++-- 2 general purpose inputs for reading the player 1 joystick
	|||| ++---- 2 general purpose inputs for reading the player 2 joystick
	|+++------- Always 0. (Strongly implied to exist on the gate array, but grounded there)
	+---------- Always 1. (Probably pulled up by resistor next to Z80)

     */
     
    // Controller stuff
    data = 0;
    for(u8 i : range(4)) {
      if (cpu.io[5] & 1 << i){
        data |= cpu.controller_read( i,0 );
        fd_data &= ~(1 << i);
      }
    }
    /* // quick little debugger
    print("0: ");
    print(cpu.controller_read( 0,0 ));
    print("\t1: ");
    print(cpu.controller_read( 1,0 ));
    print("\t2: ");
    print(cpu.controller_read( 2,0 ));
    print("\t3: ");
    print(cpu.controller_read( 3,0 ));
    print("\n");
    //*/
    break;
    
  default: // UNKNOWN IO
    print("Unknown IO Read: A:");
    print(address);
    print("\tD:");
    print(data);
    print("\n");
    break;
  }
	return data;
}
auto VDP::out(n16 address, u8 data) -> void {
  switch(address) {
    case 0xF8: // Square 1
      square_p1 = (data&0b111111);
      print("Square 1: ");
      print(square_p1);
      print("\n");
      break;
    case 0xF9: // Square 2
      square_p2 = (data&0b111111);
      print("Square 2: ");
      print(square_p2);
      print("\n");
      break;
    case 0xFA: // Square 3
      square_p3 = (data&0b111111);
      print("Square 3: ");
      print(square_p3);
      print("\n");
      break;
    case 0xFB: // Sound control [IGNORED FOR NOW]
    /*
	7  bit  0
	.... ..ES
		   ||
		   |+--- 0: Normal
		   |     1: XOR/ring modulation
		   +---- 0: Mute
				 1: Sound audible
     */
      break;

    case 0xFC: // Interrupt enable (Existing games write 0, 1, or 3. )
    /*
	7  bit  0
	.... ..MP
		   ||
		   |+--- 1: enable prerender IRQ (1/vblank) 
		   +---- 1: enable matrix scan IRQ (16/vblank)

	It is known that 0x1C bits have no effect.

	When enabled, IRQ becomes asserted at pixel 276, immediately after the last pixel of the active area is drawn.

	If the first active scanline is numbered 0,

	- the matrix scan IRQ happens at scanlines 195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255
	- the prerender IRQ happens at scanline 255 (the exact same as the last matrix scan IRQ)

	IRQs do not care if rendering is enabled. The same 16 IRQs will fire regardless.

	Disabling an interrupt also acknowledges it. 
     */
      //TODO
      break;
      
    case 0xFD: // GPO (Also acknowledges prerender IRQ. )
    /*
	7  bit  0
	.AAA BBBB
	 ||| ||||
	 ||| ++++-- 4 general purpose outputs, accessible via both controller ports
	 +++------- Strongly implied to exist on the gate array, but not accessible externally
     */
      fd_data = 0xf;
      break;
      
    case 0xFE: // Tilemap and pattern RAM address
    /*
	7  bit  0
	BBBB AA..
	|||| ||
	++++-++---- Address A15-A10 from where to fetch the tilemap. 
	||||        Almost all software write $B8, meaning the gate array fetches tilemap from $B802 through $BAFD.
	++++------- Address A15-A12 from where to fetch pattern RAM tiles (A11-A10 always 1)
				The common value of $B8 means that gate array fetches data for tiles $E0-$FF from $BC08-$BFFF
     */
      vdp.tile_bank     = (data >> 2)&3;
      vdp.tile_bank_ram = data >> 4;
      break;
      
    case 0xFF: // Display properties
    /*
	7  bit  0
	AAAD FBGR
	|||| ||||
	|||| |+++-- Border color. (0=black, 1=red, 2=green, 3=yellow, 4=blue, &c).
	|||| +----- 1: Display disable. (Screen solid border color, faster Z80)
	|||+------- Pattern RAM disable 
	|||          (0: Tiles $E0-$FF are fetched from address specified by port $FE
	|||           1: Tiles $E0-$FF are fetched from address specified by port $FF)
	+++-------- Address A15-A13 from where to fetch patterns
				 (0: Patterns are fetched from $0008-$1BFF or -$1FFF
				  1: starting at $2008
				  2: starting at $4008 &c

	Dig Dug uses both D and the lsbit of AAA.
	Enabling the display in the middle of a scanline causes 8 to 16 pixels of noise, specific origin unknown. 
     */
    
      vdp.border_col = data & 7;
      vdp.display_disable = (data>>3)&1;
      vdp.pattern_ram_disable = ((data & 0x10) >> 4); /* Dig Dug relies on this */
      vdp.patten_bank = data >> 5;
      
      print("bank: ");
      print(vdp.patten_bank);
      print("\tFP: ");
      print(vdp.pattern_ram_disable);
      print("\tBC: ");
      print(vdp.border_col);
      print("\n");
      break;
      
  default: // UNKNOWN IO
    print("Unknown IO Write: A:");
    print(address);
    print("\tD:");
    print(data);
    print("\n");
    break;
  }
}


auto VDP::frame() -> void {
  screen->frame();
  scheduler.exit(Event::Frame);
}

auto VDP::power() -> void {
  Thread::create(system.frequency()/2, [&] { main(); });
  //Thread::create(system.frequency()/2, [&] { main_sound(); });
  screen->power();
  
  hcounter = 0;
  vcounter = 0;
  pattern_ram_disable = false;
  patten_bank = 0;
  tile_bank = 0;
  
  
  square_p1 = 255;
  square_p2 = 255;
  square_p3 = 255;
}

}
