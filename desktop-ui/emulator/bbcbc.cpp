struct BBC_Bridge_Companion: Emulator {
  BBC_Bridge_Companion();
  auto load() -> bool override;
  auto save() -> bool override;
  auto pak(ares::Node::Object) -> shared_pointer < vfs::directory > override;
};

BBC_Bridge_Companion::BBC_Bridge_Companion() {
  manufacturer = "Unicard";
  name = "BBC Bridge Companion";
  
  firmware.append({"BIOS", "World", "990bf1956f10207d8781b619eb74f89b00d921c8d45c95c334c16c8cceca09ad"});
  
  {
    InputPort port {
      "BBC Bridge Companion"
    }; {
      InputDevice device {
        "Controls"
      };
      device.digital("UP [B]", virtualPorts[0].pad.up);
      device.digital("DOWN [C]", virtualPorts[0].pad.down);
      device.digital("LEFT [A]", virtualPorts[0].pad.left);
      device.digital("RIGHT [D]", virtualPorts[0].pad.right);
      device.digital("ACTION [E]", virtualPorts[0].pad.south);

      device.digital("1", virtualPorts[0].pad.east);
      device.digital("2", virtualPorts[0].pad.west);
      device.digital("3", virtualPorts[0].pad.north);
      device.digital("4", virtualPorts[0].pad.l_bumper);
      device.digital("5", virtualPorts[0].pad.r_bumper);
      device.digital("6", virtualPorts[0].pad.l_trigger);
      device.digital("7", virtualPorts[0].pad.r_trigger);
      device.digital("8", virtualPorts[0].pad.lstick_click);
      device.digital("9", virtualPorts[0].pad.rstick_click);
      device.digital("10", virtualPorts[0].pad.lstick_up);
      device.digital("11", virtualPorts[0].pad.lstick_down);
      device.digital("12", virtualPorts[0].pad.lstick_left);
      device.digital("13", virtualPorts[0].pad.lstick_right);
      device.digital("14", virtualPorts[0].pad.rstick_up);
      port.append(device);
    }

    ports.append(port);
  }
}

auto BBC_Bridge_Companion::load() -> bool {
  game = mia::Medium::create("BBC Bridge Companion");
  if (!game -> load(Emulator::load(game, configuration.game))) return false;
  
  system = mia::System::create("BBC Bridge Companion");
  if(!system->load(firmware[0].location)) return errorFirmware(firmware[0]), false;

  if (!ares::BBC_Bridge_Companion::load(root, {
      "[Unicard] BBC Bridge Companion"
    })) return false;

  if (auto port = root -> find < ares::Node::Port > ("Cartridge Slot")) {
    port -> allocate();
    port -> connect();
  }

  return true;
}

auto BBC_Bridge_Companion::save() -> bool {
  root -> save();
  system -> save(system -> location);
  game -> save(game -> location);
  return true;
}

auto BBC_Bridge_Companion::pak(ares::Node::Object node) -> shared_pointer < vfs::directory > {
  if (node -> name() == "BBC Bridge Companion") return system -> pak;
  if (node -> name() == "BBC Bridge Companion Cartridge") return game -> pak;
  return {};
}
