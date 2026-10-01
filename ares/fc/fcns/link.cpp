//the seam between the emulated modem and whatever stands in for the telephone network. The
//analogue side is not modelled; what crosses here is the byte stream the UART carries, plus the
//call setup around it. Three backends: Disconnected (nobody answers, the honest default for a
//service that stopped in 1991), Loopback (transmitted bytes come straight back), and Socket.
//
//Socket speaks one text line per event in each direction, so a peer can be written in anything:
//
//  ares -> peer   DIGIT xx    a dial digit, as the raw MSM6827 TR value
//                 DATA xx     one byte the card transmitted
//                 DIALDONE    the inter-digit timeout expired; the dialled number is complete
//                 HANGUP      the card dropped the line
//  peer -> ares   ANSWER      the far end picked up: carrier and answer tone assert
//                 DATA xx ..  bytes to hand to the card
//                 BUSY        the far end refused
//                 HANGUP      the far end dropped the line
//                 POLARITY n  line polarity, 1 = reversed (NTT's answer supervision)
//                 SDT on off cycles
//                             play the NTT second dial tone, an interrupted tone the DDX network
//                             answers an access number with. on/off in milliseconds; 0 cycles
//                             plays until stopped.
//
//unknown lines are ignored, so a peer may send its own extensions.

auto FCNS::Link::open() -> void {
  if(!target) return;
  if(target.ibeginsWith("loopback")) return (void)(mode = Mode::Loopback);
  if(socket.open(target)) mode = Mode::Socket;
}

auto FCNS::Link::connected() const -> bool {
  if(mode == Mode::Socket) return socket.opened();
  return mode != Mode::Disconnected;
}

//whether the far end has actually picked up, as opposed to merely being reachable. This is what the
//modem's carrier and answer-tone detectors follow: a peer that is connected but has not answered is
//a ringing phone, and the card should still be waiting.
auto FCNS::Link::offHook() const -> bool {
  if(mode == Mode::Loopback) return true;  //loopback answers itself
  return answered;
}

auto FCNS::Link::send(string line) -> void {
  if(mode != Mode::Socket || !socket.opened()) return;
  line.append("\n");
  socket.write((const u8*)line.data(), line.size());
}

//a real exchange timed the gap between digits to know an address was complete, so that timeout lives
//here, on the machine's own clock, and the peer is told when it expires.
auto FCNS::Link::clockDigits() -> void {
  if(!digitsSent || digitGap == ~0u) return;
  //one second of emulated time, which has to exceed the slowest inter-digit gap the dial tables can
  //produce, or one number is chopped into several: 158.3ms for DTMF, 547.5ms at 20pps, 697.5ms at
  //10pps.
  if(++digitGap < 1'789'773) return;
  digitGap = ~0u;                  //latched: one DIALDONE per address, not one per idle tick
  send("DIALDONE");
}

auto FCNS::Link::digit(n8 value) -> void {
  digitsSent++;
  digitGap = 0;
  //the raw TR value goes on the wire as well as the decoded key: the key is what a peer wants, and
  //the raw value is what makes a wrong decode visible instead of silent.
  auto key = FCNS::Modem::dtmfKey((n4)value);
  send({"DIGIT ", hex(value, 2L), " ", key});
}

auto FCNS::Link::hangup() -> void {
  if(!answered && mode != Mode::Socket) return;
  send("HANGUP");
  answered = 0;
  busy = 0;
}

auto FCNS::Link::transmit(n8 data) -> void {
  switch(mode) {
  case Mode::Disconnected:
    break;  //into the void; no carrier, nothing listening
  case Mode::Loopback:
    if(rx.size() < QueueLimit) rx.push_back(data);
    break;
  case Mode::Socket:
    send({"DATA ", hex(data, 2L)});
    break;
  }
  //kept regardless of mode so a debugger/trace can see what the card tried to say
  if(tx.size() < QueueLimit) tx.push_back(data);
}

auto FCNS::Link::receivable() const -> bool {
  return !rx.empty();
}

auto FCNS::Link::receive() -> n8 {
  if(rx.empty()) return 0xff;  //idle mark
  n8 data = rx.front();
  rx.erase(rx.begin());
  return data;
}

auto FCNS::Link::parse(string line) -> void {
  line.strip();
  if(!line) return;
  auto parts = nall::split(line, " ");
  auto command = string{parts[0]}.strip().upcase();

  if(command == "ANSWER") {
    answered = 1;
    busy = 0;
  } else if(command == "BUSY") {
    answered = 0;
    busy = 1;
  } else if(command == "HANGUP") {
    answered = 0;
  } else if(command == "POLARITY") {
    polarity = parts.size() > 1 && string{parts[1]}.strip() != "0";
  } else if(command == "SDT") {
    u32 on     = parts.size() > 1 ? string{parts[1]}.natural() :  80;   //milliseconds
    u32 off    = parts.size() > 2 ? string{parts[2]}.natural() : 120;
    u32 cycles = parts.size() > 3 ? string{parts[3]}.natural() :  36;
    //a peer may deliver the tone in chunks, but on a real line it is unbroken, and CPU2 needs six
    //consecutive cadence matches: a chunk arriving mid-tone extends it instead of restarting it.
    //a cycle count of zero stops the tone.
    if(playing() && on == sdtOn && off == sdtOff && cycles) {
      sdtCycles += cycles;
    } else {
      sdtOn = on; sdtOff = off; sdtCycles = cycles; sdtTicks = 0;
    }
  } else if(command == "DATA") {
    u32 count = 0;
    for(u32 n : range(parts.size())) {
      if(!n) continue;  //parts[0] is the command itself
      auto part = string{parts[n]}.strip();
      if(!part) continue;
      if(rx.size() >= QueueLimit) break;
      rx.push_back((n8)part.hex());
      count++;
    }
  }
  //anything else is a peer extension this end does not need to understand
}

//where in the second dial tone's pulse train we are, or false once it has run out. the cadence has
//to satisfy two gates in CPU2's detector: $eb80 wants two cycles to span 25..49 samples, and $eac8
//wants the on-run and off-run in particular buckets. the 2:3 on:off ratio is what matters.
//one tick is one Famicom CPU cycle, at 1.789773MHz
auto FCNS::Link::sdtTicksFor(u32 milliseconds) -> u32 {
  return milliseconds * 1789773 / 1000;
}

auto FCNS::Link::clock() -> void {
  if(sdtCycles) sdtTicks++;
}

//whether a tone is still running, as opposed to finished or never started. Divides rather than
//multiplies: an open-ended tone carries a cycle count large enough to overflow the product.
auto FCNS::Link::playing() const -> bool {
  auto period = sdtTicksFor(sdtOn + sdtOff);
  return sdtCycles && period && sdtTicks / period < sdtCycles;
}

auto FCNS::Link::secondDialTone() const -> bool {
  if(!playing()) return false;
  auto period = sdtTicksFor(sdtOn + sdtOff);
  return sdtTicks % period < sdtTicksFor(sdtOn);
}

//drain whatever the far end has sent since the last look. Called from the adapter's main loop, so
//it has to stay cheap and must never block; Socket::read is non-blocking for that reason.
auto FCNS::Link::poll() -> void {
  if(mode != Mode::Socket || !socket.opened()) return;

  u8 buffer[256];
  auto size = socket.read(buffer, sizeof(buffer));
  for(u32 n : range(size)) {
    if(buffer[n] == '\n') {
      parse(pending);
      pending = "";
    } else if(pending.size() < 256) {
      pending.append((char)buffer[n]);
    }
  }
}

auto FCNS::Link::reset() -> void {
  tx.clear();
  rx.clear();
  pending = "";
  answered = 0;
  busy = 0;
  polarity = 0;
  sdtOn = 0; sdtOff = 0; sdtCycles = 0; sdtTicks = 0;
}

auto FCNS::Link::serialize(serializer& s) -> void {
  //the queues are deliberately not serialized: a save state that restored half a conversation with
  //an external peer would desynchronize against that peer the moment it was loaded. State restores
  //with an idle link, which is the only self-consistent option while the far end lives in another
  //process.
  u32 m = (u32)mode;
  s(m);
  if(s.reading()) mode = (Mode)m, reset();
}
