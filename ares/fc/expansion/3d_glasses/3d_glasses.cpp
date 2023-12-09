// TODO: How do we display this?

F3D_Glasses::F3D_Glasses(Node::Port parent) {
  node = parent->append<Node::Peripheral>("3D Glasses");
}

auto F3D_Glasses::write(n8 data) -> void {
  if (data.bit(1) == lens) return;
  lens = data.bit(1); // what shutter is active.
}
