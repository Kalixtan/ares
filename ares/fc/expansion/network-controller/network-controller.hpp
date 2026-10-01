//Famicom Network Controller (HVC-051), on the expansion port.
//
//nesdev.org/wiki/Famicom_Network_Controller: it plugs into the Famicom expansion port and its serial
//output is read on $4016 bit D1 (expansion pin 13, read1). The strobe is the standard $4016 bit D0
//write (pin 12, latch). It is a standard pad that keeps shifting -- 24 bits instead of 8 -- so an
//ordinary game reading eight bits sees an ordinary pad, and past the 24th bit the line reads 1.
//
//  bit  0-7 : A, B, Select, Start, Up, Down, Left, Right   (standard pad order)
//  bit 8-17 : digits 0..9
//  bit   18 : *
//  bit   19 : #
//  bit   20 : .
//  bit   21 : C   (clear)
//  bit   22 : always 0
//  bit   23 : End Communication (通信終了); the cards prompt for this as the Execute key
//
//This is the same protocol as the old controller-port NetworkController, moved here because the card
//software reads it at $4016 D1 -- Super Mario Club's $dc6f loop shifts D0 into the standard-pad slot
//and D1 into the keypad slot on every read, and JRA-PAT's $e150 loop reads D1 only and refuses to run
//if nothing answers there. On the controller port (D0) the keypad never reached D1 and JRA-PAT failed.
struct NetworkController : Expansion {
  Node::Input::Button up, down, left, right, b, a, select, start;
  Node::Input::Button digit[10];
  Node::Input::Button asterisk, hash, period, clear, execute;

  NetworkController(Node::Port);
  auto read1() -> n1 override;   //$4016 D1
  auto write(n8 data) -> void override;
  auto serialize(serializer&) -> void override;

private:
  auto poll() -> void;

  n1 latched;
  n8 counter;
  u32 probeCount = 0;  //bring-up: ARES_FCNS_CTRLPROBE logs the read sequence
  n1 state[24];
};
