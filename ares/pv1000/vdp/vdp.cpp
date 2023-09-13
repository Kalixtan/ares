#include <pv1000/pv1000.hpp>

namespace ares::PV1000 {

VDP vdp;
#include "color.cpp"
#include "serialization.cpp"

auto VDP::main() -> void {

  if (true) {
    const auto pixel = (vcounter * screen->width()) + hcounter;
    const auto tile_index  = ((vcounter>>3) * 32) + (hcounter>>3);
    auto buffer = screen->pixels().data();
    
    // get tile
    auto tile = cpu.ram.read(tile_index);
    //tile = tile_index; // testing
    auto cart_tile = tile < 0xe0 || force_pattern;
    
    if ( cart_tile ){
		tile += ( (pcg_bank+1) << 8);
	} else {
		tile -= 0xe0;
	}
	
    // do tile
    const auto offset = (tile*4*8)+8+(vcounter%8);
	auto g1 = cpu.ram.read(offset+0x400+0 );
	auto g2 = cpu.ram.read(offset+0x400+8 );
	auto g3 = cpu.ram.read(offset+0x400+16);
    
    if ( cart_tile ){
		tile += ( pcg_bank << 8);
      g1 = cartridge.read(offset+0 );
      g2 = cartridge.read(offset+8 );
      g3 = cartridge.read(offset+16);
	}
    
    const auto bit = 7-(hcounter%8);
    buffer[pixel] = ((g1>>bit)&1) | (((g2>>bit)&1)<<1) | (((g3>>bit)&1)<<2);
    
  }
  step(1);

  
  // Increment pixel counters
  hcounter++;
  if (hcounter == 32*8) {
    hcounter = 0;
    vcounter++;
    if (vcounter == 24*8) {
      vcounter = 0;
      
      cpu.setIRQ(true);
      cpu.buffer_flag |= 1;
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
}

auto VDP::unload() -> void {
  screen->quit();
  node->remove(screen);
  screen.reset();
  node.reset();
}

auto VDP::step(u32 clocks) -> void {
  Thread::step(clocks);
  Thread::synchronize(cpu);
}

auto VDP::irq(bool line) -> void {
  cpu.setIRQ(line);
}

auto VDP::frame() -> void {
  screen->frame();
  scheduler.exit(Event::Frame);
}

auto VDP::power() -> void {
  Thread::create(192000, [&] { main(); });
  screen->power();
  
  hcounter = 0;
  vcounter = 0;
  force_pattern = false;
  pcg_bank = 0;
}

}
