#include <apple1/apple1.hpp>

namespace ares::Apple1 {

Cartridge& cartridge = cartridgeSlot.cartridge;
#include "board/board.cpp"
#include "slot.cpp"
#include "serialization.cpp"

auto Cartridge::allocate(Node::Port parent) -> Node::Peripheral {
  return node = parent->append<Node::Peripheral>(string{system.name(), " Cartridge"});
}

auto Cartridge::connect() -> void {
  if(!node->setPak(pak = platform->pak(node))) return;

  information = {};
  information.title  = pak->attribute("title");
  information.board  = pak->attribute("board");

  if(information.board == "Linear") board = new Board::Linear{*this};

  if(!board) board = new Board::Interface{*this};
  board->pak = pak;
  board->load();
  power();
}

auto Cartridge::disconnect() -> void {
  if(!node || !board) return;
  board->unload();
  board->pak.reset();
  board.reset();
  node.reset();
}

auto Cartridge::save() -> void {
  if(!node) return;
  if(board) board->save();
}

auto Cartridge::power() -> void {
  if(board) board->power();
}

auto Cartridge::read(n16 address) -> n8 {
  if(!node) return 0xff;
  if(board) return board->read(address);

  return 0xff;
}

auto Cartridge::write(n16 address, n8 data) -> bool {
  if(!node) return false;
  if(board) return board->write(address, data);
  return false;
}

}
