struct PV1000: Emulator {
  PV1000();
  auto load() -> bool override;
  auto save() -> bool override;
  auto pak(ares::Node::Object) -> shared_pointer < vfs::directory > override;
};

PV1000::PV1000() {
  manufacturer = "Casio";
  name = "PV-1000";
  {
    InputPort port {
      "PV-1000"
    }; {
      InputDevice device {
        "Controls"
      };
      u8 id = 0; // TODO: work out how to add the other player
    device.digital("Up",         virtualPorts[id].pad.up);
    device.digital("Down",       virtualPorts[id].pad.down);
    device.digital("Left",       virtualPorts[id].pad.left);
    device.digital("Right",      virtualPorts[id].pad.right);
    device.digital("B",          virtualPorts[id].pad.west);
    device.digital("A",          virtualPorts[id].pad.south);
    device.digital("Select",     virtualPorts[id].pad.select);
    device.digital("Start",      virtualPorts[id].pad.start);
      
      port.append(device);
    }

    ports.append(port);
  }
}

auto PV1000::load() -> bool {
  game = mia::Medium::create("PV-1000");
  if (!game -> load(Emulator::load(game, configuration.game))) return false;

  system = mia::System::create("PV-1000");
  if(!system->load()) return false;

  if (!ares::PV1000::load(root, {
      "[Casio] PV-1000"
    })) return false;

  if (auto port = root -> find < ares::Node::Port > ("Cartridge Slot")) {
    port -> allocate();
    port -> connect();
  }

  return true;
}

auto PV1000::save() -> bool {
  root -> save();
  system -> save(system -> location);
  game -> save(game -> location);
  return true;
}

auto PV1000::pak(ares::Node::Object node) -> shared_pointer < vfs::directory > {
  if (node -> name() == "PV-1000") return system -> pak;
  if (node -> name() == "PV-1000 Cartridge") return game -> pak;
  return {};
}
