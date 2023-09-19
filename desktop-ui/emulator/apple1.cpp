struct Apple1: Emulator {
  Apple1();
  auto load() -> bool override;
  auto save() -> bool override;
  auto pak(ares::Node::Object) -> shared_pointer < vfs::directory > override;
};

Apple1::Apple1() {
  manufacturer = "Apple";
  name = "Apple 1";
  
  firmware.append({"BIOS", "Wozmon", "e5af0d1c4057bd8e0ef5cb069c208ff7cc0984a7dff53b12c5cf119de8cb5c25"});
  
  {
    InputPort port {
      "Apple 1"
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

auto Apple1::load() -> bool {
  game = mia::Medium::create("Apple 1");
  if (!game -> load(Emulator::load(game, configuration.game))) return false;

  system = mia::System::create("Apple 1");
  if(!system->load(firmware[0].location)) return errorFirmware(firmware[0]), false;

  if (!ares::Apple1::load(root, {
      "[Apple] Apple 1"
    })) return false;

  if (auto port = root -> find < ares::Node::Port > ("Cartridge Slot")) {
    port -> allocate();
    port -> connect();
  }

  return true;
}

auto Apple1::save() -> bool {
  root -> save();
  system -> save(system -> location);
  game -> save(game -> location);
  return true;
}

auto Apple1::pak(ares::Node::Object node) -> shared_pointer < vfs::directory > {
  if (node -> name() == "Apple 1") return system -> pak;
  if (node -> name() == "Apple 1 Cartridge") return game -> pak;
  return {};
}
