struct MegaDuck : Emulator {
  MegaDuck();
  auto load() -> bool override;
  auto save() -> bool override;
  auto pak(ares::Node::Object) -> shared_pointer<vfs::directory> override;
};

MegaDuck::MegaDuck() {
  manufacturer = "Nintendo";
  name = "MegaDuck";

  { InputPort port{"MegaDuck"};

  { InputDevice device{"Controls"};
    device.digital("Up",      virtualPorts[0].pad.up);
    device.digital("Down",    virtualPorts[0].pad.down);
    device.digital("Left",    virtualPorts[0].pad.left);
    device.digital("Right",   virtualPorts[0].pad.right);
    device.digital("B",       virtualPorts[0].pad.south);
    device.digital("A",       virtualPorts[0].pad.east);
    device.digital("Select",  virtualPorts[0].pad.select);
    device.digital("Start",   virtualPorts[0].pad.start);
    device.rumble ("Rumble",  virtualPorts[0].pad.rumble);
    port.append(device); }

    ports.append(port);
  }
}

auto MegaDuck::load() -> bool {
  game = mia::Medium::create("MegaDuck");
  if(!game->load(Emulator::load(game, configuration.game))) return false;

  system = mia::System::create("MegaDuck");
  if(!system->load()) return false;

  if(!ares::GameBoy::load(root, "[Nintendo] MegaDuck")) return false;

  if(auto port = root->find<ares::Node::Port>("Cartridge Slot")) {
    port->allocate();
    port->connect();
  }

  if(auto fastBoot = root->find<ares::Node::Setting::Boolean>("Fast Boot")) {
    fastBoot->setValue(settings.boot.fast);
  }

  return true;
}

auto MegaDuck::save() -> bool {
  root->save();
  system->save(system->location);
  game->save(game->location);
  return true;
}

auto MegaDuck::pak(ares::Node::Object node) -> shared_pointer<vfs::directory> {
  if(node->name() == "MegaDuck") return system->pak;
  if(node->name() == "MegaDuck Cartridge") return game->pak;
  return {};
}
