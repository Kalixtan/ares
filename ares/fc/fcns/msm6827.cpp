//Oki MSM6827L: the V.21/V.22/Bell 103/212A modem inside the RF5A18. The serial variant, with four
//registers selected by A1:A0 and bytes shifted LSB-first on EXCLK. CPU2 bit-bangs that through two
//of its own registers:
//
//  $4120  bit 0 = A0, bit 1 = A1, bit 3 = /RD, bit 4 = /WR, bit 5 = EXCLK
//  $4121  the byte shifted out or in
//
//the shift register is not modelled bit by bit: the transfer completes on the /WR and /RD edges,
//with $4121 as the payload either way.

//The DTMF tone frequency setting table, from the datasheet. Indexed by TR's low nibble.
auto FCNS::Modem::dtmfKey(n4 data) -> char {
  static const char keys[16] = {'D', '1', '2', '3', '4', '5', '6', '7',
                                '8', '9', '0', '*', '#', 'A', 'B', 'C'};
  return keys[data];
}

auto FCNS::Modem::registerIndex(n8 control) const -> n2 {
  return control.bit(0) | control.bit(1) << 1;
}

//DR is read-only and reports the detectors.
auto FCNS::Modem::status() const -> n8 {
  n8 data;
  data.bit(7) = 1;  //chip identification code: 1 in D7, 0 in D6
  data.bit(6) = 0;
  data.bit(5) = receiveData;
  //D4 reports data mode. NTT signalled that by reversing line polarity, which is what $ec62 waits
  //for before the connection is considered up.
  data.bit(4) = rdlRequest | fcns.link.reversed();
  //carrier and answer tone follow whether the far end has actually picked up; with nothing attached
  //they never assert, so the cards take their own "no connection" paths.
  data.bit(3) = carrierDetect | fcns.link.offHook();
  data.bit(2) = answerTone | fcns.link.offHook();
  //D1 is call progress. A real line gives dial tone from the moment the handset lifts until the first
  //digit goes out; the script waits for it at $ea9b and for it to stop at $e933. The NTT second dial
  //tone arrives here too, as an interrupted tone the peer plays.
  data.bit(1) = callProgress | fcns.link.secondDialTone()
              | (tonePresent && !dialStarted);
  //D0 is long-loop detect, asserted while on hook: no loop current to measure.
  data.bit(0) = longLoop | !fcns.cpu2.offHook();
  return data;
}

auto FCNS::Modem::write(n16 address, n8 data) -> void {
  if(address == 0x4121) { latch = data; return; }
  if(address != 0x4120) return;

  n1 rd = !data.bit(3);  //active low
  n1 wr = !data.bit(4);

  //a completed /WR strobe (asserted, then released) commits the latched byte
  if(wrAsserted && !wr) {
    static const string names[4] = {"CR0", "CR1", "DR", "TR"};
    switch(registerIndex(data)) {
    case 0: {
      //"Tone dialing starts when the DTMF tone send mode is selected and the transmit enable (TX EN)
      //changes from logical 0 to 1" -- so one dialled digit is a rising edge of CR0's TX EN while
      //TR's TX DTMF is set. TR's low nibble is the digit; the datasheet's DTMF tone frequency
      //setting table gives the key, and it is not the obvious mapping (0 is $a, not $0).
      n8 previous = cr0;
      cr0 = latch;
      if(cr0.bit(1) && !previous.bit(1) && tr.bit(4)) { dialStarted = 1; fcns.link.digit(tr); }
      break;
    }
    case 1:
      cr1 = latch;
      //CR1.D2 is RESET: powers the modem down and clears CR0, CR1 and TR
      if(cr1.bit(2)) { cr0 = 0; cr1 = 0; tr = 0; }
      break;
    case 2: break;  //DR is read-only
    case 3: tr = latch; break;
    }
  }
  ///RD asserted: present the selected register so the following EXCLK pulses shift it out. Bit 6
  //is the interface's data direction -- 1 = input, 0 = output -- and the ROM sets it accordingly:
  //its write routine at $e3b8 drives $38 (bit 6 clear) and its read routine at $e3e7 drives $5a
  //then $52 (bit 6 set). So a read only presents a register when the bus is turned around.
  if(!rdAsserted && rd && data.bit(6)) {
    switch(registerIndex(data)) {
    case 0: latch = cr0; break;
    case 1: latch = cr1; break;
    case 2: {
      latch = status();
      break;
    }
    case 3: latch = tr; break;
    }
  }

  wrAsserted = wr;
  rdAsserted = rd;
  control = data;
}

//$4121 echoes the last value written. On hardware a read returns whatever the selected register
//drives; the per-register read semantics are not known, so the echo stands in for them.
auto FCNS::Modem::read(n16 address) -> n8 {
  if(address == 0x4121) return latch;
  if(address == 0x4120) return control;
  return 0x00;
}

//Clocked once per Famicom CPU cycle. The line going off hook starts a delay; the tone appears when
//it expires and stays until the first digit goes out. Going back on hook resets the whole thing.
auto FCNS::Modem::clock() -> void {
  //on hook there is no line to listen to; going back on hook clears the tone and rearms the delay
  if(!fcns.cpu2.offHook()) { toneDelay = 0; tonePresent = 0; dialStarted = 0; return; }
  if(tonePresent) return;
  //the datasheet's call-progress tone detection delay: 50ms
  if(++toneDelay >= 89'489) tonePresent = 1;
}

auto FCNS::Modem::power() -> void {
  cr0 = 0; cr1 = 0; tr = 0; latch = 0; control = 0;
  wrAsserted = 0; rdAsserted = 0;
  //nothing is plugged into the phone jack
  carrierDetect = 0; answerTone = 0; callProgress = 0; longLoop = 0;
  dialStarted = 0;
  tonePresent = 0; toneDelay = 0;
  receiveData = 1;  //idle mark
  rdlRequest = 0;
}

auto FCNS::Modem::serialize(serializer& s) -> void {
  s(cr0); s(cr1); s(tr); s(latch); s(control);
  s(wrAsserted); s(rdAsserted);
  s(carrierDetect); s(answerTone); s(callProgress); s(longLoop);
  s(dialStarted); s(tonePresent); s(toneDelay);
  s(receiveData); s(rdlRequest);
}
