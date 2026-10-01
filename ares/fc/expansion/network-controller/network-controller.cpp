NetworkController::NetworkController(Node::Port parent) {
  node = parent->append<Node::Peripheral>("Network Controller");

  up     = node->append<Node::Input::Button>("Up");
  down   = node->append<Node::Input::Button>("Down");
  left   = node->append<Node::Input::Button>("Left");
  right  = node->append<Node::Input::Button>("Right");
  b      = node->append<Node::Input::Button>("B");
  a      = node->append<Node::Input::Button>("A");
  select = node->append<Node::Input::Button>("Select");
  start  = node->append<Node::Input::Button>("Start");

  static const string names[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
  for(u32 n : range(10)) digit[n] = node->append<Node::Input::Button>(names[n]);

  asterisk = node->append<Node::Input::Button>("Asterisk");
  hash     = node->append<Node::Input::Button>("Hash");
  period   = node->append<Node::Input::Button>("Period");
  clear    = node->append<Node::Input::Button>("Clear");
  //panel legend is 通信終了; the cards prompt for it as the Execute key
  execute  = node->append<Node::Input::Button>("Execute");
}

auto NetworkController::poll() -> void {
  for(auto& button : {a, b, select, start, up, down, left, right,
                      asterisk, hash, period, clear, execute}) {
    platform->input(button);
  }
  for(u32 n : range(10)) platform->input(digit[n]);

  state[ 0] = a->value();
  state[ 1] = b->value();
  state[ 2] = select->value();
  state[ 3] = start->value();
  //no opposing-direction arbitration here, unlike Gamepad: this is a real cross pad on a keypad
  //housing and cannot physically report up+down or left+right at once.
  state[ 4] = up->value();
  state[ 5] = down->value();
  state[ 6] = left->value();
  state[ 7] = right->value();
  for(u32 n : range(10)) state[8 + n] = digit[n]->value();
  state[18] = asterisk->value();
  state[19] = hash->value();
  state[20] = period->value();
  state[21] = clear->value();
  state[22] = 0;
  state[23] = execute->value();
}

//$4016 D1 (expansion pin 13, read1).
auto NetworkController::read1() -> n1 {
  //while the latch is held high the pad reports its first bit continuously
  if(latched == 1) {
    platform->input(a);
    n1 v = a->value();
    return v;
  }
  //past the 24th bit the data line idles high, the same as a standard pad past bit 8
  n1 v = counter >= 24 ? (n1)1 : state[counter];
  if(counter < 24) counter++;
  return v;
}

//$4016 D0 is the latch/strobe (expansion pin 12).
auto NetworkController::write(n8 data) -> void {
  n1 value = data.bit(0);
  if(latched == value) return;
  latched = value;
  counter = 0;
  if(latched == 0) poll();
}

auto NetworkController::serialize(serializer& s) -> void {
  s(latched);
  s(counter);
  s(state);
}
