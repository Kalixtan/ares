//The RF5A18's on-chip UART, at $4110-$4114. This is the actual serial data path: the $4120/$4121
//pair elsewhere is only the MSM6827's bit-banged *register* interface, which configures the modem
//but never carries a payload byte. Everything below is the register documentation from
//nesdev.org/wiki/Family_Computer_Network_System; see reference/FCNS.md for the tables.
//
//The modem is a 1200bps V.22/BELL 212A part and CPU2 programs it for PSK asynchronous, 10 bits per
//character, so a frame is start + 8 data + stop with no parity -- but the UART is more capable than
//that and the ROM does reconfigure it, so the framing is computed from the registers rather than
//assumed.

auto FCNS::CPU2::UART::baud() const -> u32 {
  //$4114 bits 1-0 select within a range that $4111 bit 2 halves twice over. Clearing the scaler bit
  //"multiplies the baudrate x4", so the table is one sequence read from two different offsets.
  static const u32 rates[8] = {1200, 2400, 4800, 9600, 300, 600, 1200, 2400};
  return rates[(config.bit(2) << 2) | (n2)baudSelect];
}

//Bits on the wire for one character, which is what decides how long a byte takes.
auto FCNS::CPU2::UART::frameBits() const -> u32 {
  u32 bits = 1;                       //start
  bits += config.bit(3) ? 8 : 7;      //data
  bits += config.bit(5) ? 1 : 0;      //parity, when enabled
  bits += config.bit(4) ? 2 : 1;      //stop
  return bits;
}

auto FCNS::CPU2::UART::framePeriod() const -> u32 {
  auto rate = baud();
  if(!rate) return 0;
  return (u32)(2'457'600ull * frameBits() / rate);
}

auto FCNS::CPU2::UART::read(n16 address) -> n8 {
  switch(address) {
  case 0x4110: {
    //"Reading a byte from this register automatically clears $4112.0 read value"
    rxReady = 0;
    return rxData;
  }
  case 0x4111: return config;
  case 0x4113: return config2;
  case 0x4112: {
    n8 data;
    data.bit(0) = rxReady;
    data.bit(1) = txReady;
    data.bit(2) = txIdle;
    data.bit(4) = parityError;
    data.bit(5) = framingError;
    data.bit(6) = breakReceived;
    return data;
  }
  case 0x4114: return baudSelect;
  }
  return 0x00;
}

auto FCNS::CPU2::UART::write(n16 address, n8 data) -> void {
  switch(address) {
  case 0x4110:
    //"Writing a byte to this register automatically clears $4112.1 read value and triggers the byte
    //to be sent on UART Tx". Holding it in a shift register for a whole frame time is what makes
    //the ROM's own flow control mean anything.
    txData = data;
    txReady = 0;
    txIdle = 0;
    txCounter = framePeriod();
    break;
  case 0x4111:
    config = data;
    break;
  case 0x4112:
    //"Error and Break bits persist through $4110 operations, but can be cleared by writing 1 to
    //$4112.7" -- so the status register is read-mostly rather than read-only, and this is the only
    //way the parity, framing and break flags ever go away.
    if(data.bit(7)) { parityError = 0; framingError = 0; breakReceived = 0; }
    break;
  case 0x4113:
    //A second configuration register the wiki's table does not break down: "bits 7 and 6 differ in
    //these writes, suggesting that they exist", with bit 7 said to control transmit repeat
    //behaviour feeding the Famicom-readable $40d6. The ROM writes it from six sites. Latched so the
    //value is at least observable rather than discarded.
    config2 = data;
    break;
  case 0x4114:
    baudSelect = data;
    break;
  }
}

//Clocked once per CPU2 cycle. Transmission completes a frame time after the byte was handed over,
//and reception takes a frame time from the moment a byte is available on the link.
//bring-up: which UART configuration CPU2 actually programs, and when. $e1bc builds the frame format
//with Rx and Tx both DISABLED (it returns #$04 plus data/stop/parity bits, never bit 0 or 1); the
//enables come later and separately -- $ee7e `ORA #$01` for Rx on entry to sub-state 3, $ed4e
//`ORA #$03` for both, $f0e8 `AND #$fe` to drop Rx again. So a run where nothing is received is
//ambiguous between "never enabled" and "enabled but the byte pipeline is broken", and this says which.
auto FCNS::CPU2::UART::trace(n16 address, n8 data) -> void {
  static const char* names[5] = {"data", "config", "status", "config2", "baud"};
}

auto FCNS::CPU2::UART::step() -> void {
  if(txCounter && !--txCounter) {
    txIdle = 1;
    txReady = 1;
    if(config.bit(1)) fcns.link.transmit(txData);  //bit 1 is Tx enable
  }

  if(!config.bit(0)) return;  //bit 0 is Rx enable
  if(rxReady) return;         //the buffer still holds a byte the ROM has not taken
  if(rxCounter) {
    if(--rxCounter) return;
    rxData = rxShift;
    rxReady = 1;
    return;
  }
  if(fcns.link.receivable()) {
    rxShift = fcns.link.receive();
    rxCounter = framePeriod();
  }
}

auto FCNS::CPU2::UART::power() -> void {
  config = 0; config2 = 0; baudSelect = 0;
  rxData = 0; txData = 0; rxShift = 0;
  rxReady = 0; txReady = 1; txIdle = 1;
  parityError = 0; framingError = 0; breakReceived = 0;
  txCounter = 0; rxCounter = 0;
}

auto FCNS::CPU2::UART::serialize(serializer& s) -> void {
  s(config); s(config2); s(baudSelect);
  s(rxData); s(txData); s(rxShift);
  s(rxReady); s(txReady); s(txIdle);
  s(parityError); s(framingError); s(breakReceived);
  s(txCounter); s(rxCounter);
}
