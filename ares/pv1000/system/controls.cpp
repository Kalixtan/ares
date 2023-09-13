auto System::Controls::load(Node::Object parent) -> void {
  node = parent->append<Node::Object>("Controls");

  Select = node->append<Node::Input::Button>("Select");
  Start  = node->append<Node::Input::Button>("Start");
  
  A      = node->append<Node::Input::Button>("A");
  B      = node->append<Node::Input::Button>("B");
  
  Up     = node->append<Node::Input::Button>("Up");
  Down   = node->append<Node::Input::Button>("Down");
  Left   = node->append<Node::Input::Button>("Left");
  Right  = node->append<Node::Input::Button>("Right");
}

auto System::Controls::poll() -> void {
  
  platform->input(Select);
  platform->input(Start);
  
  platform->input(A);
  platform->input(B);
  
  platform->input(Up);
  platform->input(Down);
  platform->input(Left);
  platform->input(Right);
}
