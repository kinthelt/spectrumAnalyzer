# Installation checklist

Moving the bench build into the ceiling: the circuit onto the Perma-Proto board, power fed to
both ends of the strip, the microphone through a ceiling tile. Parts, pins and power rules are
in [DESIGN.md](DESIGN.md) section 2; this list follows them.

Tick each box as you go. A multimeter is needed for sections 4 and 6.

---

## 1. Before anything comes apart (on the bench)

- [ ] The board runs the current firmware from `main` (Settings → Status → Firmware: 0.10.3 or
      later), and Apple Home and Google Home both control it.
- [ ] Note the lights' IP address (Settings, or Share). Set a **DHCP reservation** for it on the
      router, so the Share QR code and `tools/build.sh ota <ip>` keep working.
- [ ] Photograph the bench wiring from a couple of angles, for reference while rebuilding.
- [ ] Check that firmware updates work over Wi-Fi (`tools/build.sh ota`). Once the board is in
      the ceiling, that's the only way to update it.

## 2. Build the Perma-Proto board

Same circuit as the bench. Check each connection off as it's soldered.

**Level shifter, SN74AHCT126N (14-pin DIP)**

- [ ] Pin 14 (VCC) → +5 V. Pin 7 → GND.
- [ ] 0.1 µF ceramic capacitor from pin 14 to pin 7, as close to the chip as possible.
- [ ] Pin 1 (1OE) → +5 V (channel 1 always enabled; the '126's enable is active high).
- [ ] Pin 2 (1A) ← ESP32 **GPIO 7**, with the **10 kΩ** from pin 2 to GND (keeps the strip
      quiet while the ESP32 starts).
- [ ] Pin 3 (1Y) → **330 Ω** → strip data in (DIN).
- [ ] Unused inputs, pins 5 (2A), 9 (3A) and 12 (4A), → GND. Floating CMOS inputs can
      oscillate and draw current. Leave their outputs (6, 8, 11) unconnected.

**ESP32 board**

- [ ] **5V pin** ← +5 V and **GND** ← GND, taken from the Wago junction where the supply
      arrives, not from the far end of the strip (the board's 1117 regulator needs ~4.4 V).
- [ ] Consider female headers so the ESP32 board can be unplugged from the Perma-Proto for
      a USB reflash on the bench.

**Microphone (Adafruit 6049)**

- [ ] 3V → ESP32 **3V3**. GND → GND.
- [ ] BCLK → **GPIO 4**. WS / LRCL → **GPIO 5**. DOUT → **GPIO 6**.
- [ ] SEL → GND (left channel).
- [ ] Keep these wires short (ideally under 30 cm). The bit clock runs at about 3 MHz.
      Twisting the signal wires with a ground wire helps.

**Finishing**

- [ ] Strain relief on every wire leaving the board (cable ties to the board's holes, or hot
      glue), so a tug on a wire can't pull on a solder joint.
- [ ] Visual check under good light: no solder bridges, especially between neighbouring
      header pins and on the level shifter.

## 3. Wiring in the ceiling

Only 5 V goes up into the ceiling. The power supply stays at the outlet, switched at the plug.

- [ ] **14 AWG pair** from the supply up the corner to the two Wago 221-415s (+5 V and GND)
      near the strip's **left end** (the data-in end). Mark or colour-code the polarity on
      both ends of every run.
- [ ] **Strip left end:** its +5 V and GND wires into the Wagos.
- [ ] **14 AWG pair** from the Wagos along to the strip's **right end**, into its +5 V and GND.
- [ ] **1000 µF capacitor at each strip end**, across +5 V and GND, as close to the strip as
      possible. Mind the polarity: the stripe is the **negative** lead, to GND.
- [ ] **ESP32 board** power: 20 AWG from the Wagos to the board's 5V and GND.
- [ ] **Data:** the 330 Ω end to the strip's left-end **DIN** (the strip's arrows point away
      from it). Keep this wire short (under about 1 m) and run a ground wire alongside it.
- [ ] **Strip right end:** connect **power only**. Leave its data-out wire unconnected, and
      insulate the bare end.
- [ ] **Microphone:** a small hole through the tile; the mic hangs just below it. Make sure the
      sound port (on most I2S breakouts, a small hole through the board) faces the room and
      isn't covered. Strain relief above the tile.
- [ ] Board placed where air can get at it, not lying on a light fixture or a hot duct.
- [ ] Low-voltage wiring in a ceiling space: check what your local electrical code asks of it
      (the cable type, and how it's supported).
- [ ] **USB unplugged from the ESP32, and left unplugged from now on.** It's powered from its
      5V pin now, and back-feed protection between the two was never confirmed on this
      board.

## 4. Before the first power-up (supply unplugged)

- [ ] Resistance between +5 V and GND at the Wagos: **not a short**. With the capacitors it
      may start low and climb over a few seconds; a steady reading near 0 Ω is a short.
- [ ] Same check across the strip's right-end power wires, and across the ESP32's 5V and GND
      pins.
- [ ] Polarity: continuity from the supply's + to the strip's +5 V at **both** ends and to the
      ESP32's 5V pin; the same for GND.
- [ ] No stray wire strands at the Wagos; the right end's data wire is insulated.
- [ ] USB is unplugged from the ESP32.

## 5. First power-up

- [ ] Switch on at the plug. Expected: **the strip stays dark for about 5 seconds**, while
      Wi-Fi connects and Matter starts, then comes on in its last mode.
- [ ] The web page loads (`http://spectrum.local` or the IP). Settings → Status:
  - [ ] Firmware is the version from section 1.
  - [ ] Wi-Fi signal **-75 dBm or better** (the ceiling may be worse than the bench).
  - [ ] Memory: the "lowest" figure is several tens of KB (bench: about 45 KB).
- [ ] Apple Home and Google Home both control the lights. Pairings survive installation; no
      re-pairing should be needed.
- [ ] With the lights off, measure +5 V at the Wagos and at the right end: both about 5.0 V.

## 6. Full-power test

The firmware limits the strip to **5 A**, leaving 1 A of the 6 A supply for the ESP32.

- [ ] Mode **Solid**, colour **white**, brightness up in a few steps to full.
- [ ] At full brightness, measure +5 V at the **right end** of the strip and at the **ESP32's
      5V pin**. Both should stay at or above about **4.5 V**.
- [ ] The two ends of the strip look the same colour (a yellowish far end means its feed
      isn't carrying its share).
- [ ] Leave it at full white for **10–15 minutes**, then touch-check the power supply, the
      14 AWG runs, the Wagos, both strip ends and the ESP32 board. Warm is fine; too hot to
      keep a finger on means switch off and find out why.
- [ ] Brightness back to normal.

## 7. Microphone and spectrum

- [ ] Settings → Microphone: "Quiet" in a silent room, "Hearing sound" with music playing.
- [ ] With the room quiet (no music, TV off), run **Learn room noise**. The new spot has its own
      background noise (fans, the TV's standby buzz), and the lights relearn it in about 10 s.
- [ ] Play music: the spectrum follows it, with bass at the bass end. If bass is on the wrong
      side, flip **Bass end** (Settings → Strip setup); it applies at once.
- [ ] If the strip was cut to fit, set **Number of LEDs** to match (Strip setup; restarts the
      lights).

## 8. Recovery, tested in place

None of these need a ladder.

- [ ] One firmware update over Wi-Fi: `tools/build.sh ota` (or the Settings page). The strip
      turns dim blue, flashes green and restarts.
- [ ] Optional: the power-cycle trick (README → *Getting back into Wi-Fi setup*): on ~2 s,
      off, on ~2 s, off, on. **Spectrum-Setup** appears; ignore it, and the lights rejoin the
      home Wi-Fi by themselves after 10 minutes.

## 9. Done

- [ ] Share the lights from a guest phone: Settings → Share → scan the QR code from the other
      phone; the page opens and works.
- [ ] Put the sleeve/cover back over the strip's end wiring.
- [ ] From now on, the serial log isn't reachable. The Settings page shows what it used to:
      version, Wi-Fi signal, memory, audio load.
