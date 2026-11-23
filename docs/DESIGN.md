# spectrumAnalyzer: design

Status: **draft for review**. Nothing here is built yet except the bench prototype.

A 5 m strip of 150 LEDs mounted along the top of a wall, driven by an ESP32-S3. In the
spectrum modes each LED is one frequency band (bass on the left, treble on the right) and
its brightness follows that band's volume, heard through a microphone in the ceiling. Other
modes turn the strip into ordinary decorative lighting. It is controlled from a web page at
`spectrum.local`, and from Apple Home and Google Home through Matter.

---

## 1. Scope of version 1

| Area | In v1 |
|---|---|
| Modes | Spectrum Snappy, Spectrum Smooth, Solid colour, Gradient, Rainbow, Twinkle |
| Spectrum colour | Whole strip in one hue that slowly cycles through the rainbow |
| Silence | Spectrum modes fade to dark after a short silence and come back on sound |
| Presets | 8 slots, each holding mode, colours and brightness |
| Power-on | Restores the last mode and settings |
| Web page | `http://spectrum.local`, no password |
| Wi-Fi setup | Captive-portal setup network when no Wi-Fi is configured or reachable |
| Smart home | Matter colour light: on/off, brightness, colour in Apple Home and Google Home |
| Updates | Over Wi-Fi; USB is never connected once installed |

Not in v1: music-genre detection, extra effects, live spectrum preview on the web page,
scheduling. They can be added later without changing the structure below.

---

## 2. Hardware (as built and tested on the bench)

| Part | Detail |
|---|---|
| Controller | Freenove ESP32-S3-WROOM board, module N8R8 (8 MB flash, 8 MB octal PSRAM), 1117-3.3 regulator |
| LEDs | Adafruit 1376: 5 m, 30 LED/m, 150 × WS2812B/SK6812 RGB, colour order GRB |
| Microphone | Adafruit 6049, TDK ICS-43434 I2S, SEL tied to GND (left channel) |
| Level shifter | TI SN74AHCT126N (OE active high, tied to +5 V), 330 Ω series resistor, 10 kΩ pull-down on input |
| Power | Mean Well GST60A05-P1J, 5 V 6 A, at the floor outlet, switched at the plug |
| Distribution | Two Wago 221-415 (+5 V, GND); 1000 µF at each strip end; strip fed at both ends |

### Pins

| GPIO | Use |
|---|---|
| 4 | Mic BCLK |
| 5 | Mic WS (LRCL) |
| 6 | Mic DOUT |
| 7 | LED data → level shifter pin 2 |

Avoided: 0, 3, 45, 46 (strapping), 19/20 (USB), 35–37 (octal PSRAM), 38–40 (SD card),
43/44 (serial), 48 (onboard RGB LED), 2 (onboard LED).

### Power rules

- Installed: the ESP32 is powered from +5 V through its 5V pin and **USB is never connected**
  (no back-feed protection was confirmed on this board). On the bench: USB only, 5V pin empty.
- The 1117 regulator needs about 4.4 V in, so the ESP32 takes its 5 V from the junction where
  the supply arrives, not from the strip's far end.
- Long power runs (supply up the corner, Wagos to the strip's far end) use 14 AWG. Short links
  use 20 AWG solid.

### Bench measurements

| Measurement | Value |
|---|---|
| Voltage drop along the strip, ~1 A, fed from one end | 0.16 V (strip ≈ 0.3 Ω round trip) |
| Drop in bench jumper wiring, supply to Wagos, ~1 A | ~0.55 V (to be removed by proper wiring) |
| Mic level, quiet room | about −75 dBFS RMS |
| Mic level, music at bench distance | −70 to −45 dBFS RMS |
| Mic level, talking / clap | about −40 / −40 to −10 dBFS RMS |

---

## 3. Firmware structure

```
Core 0                          Core 1                                    Network (Arduino loop / tasks)
┌──────────────┐  levels[150]  ┌──────────────┐  CRGB[150]  ┌────────┐   ┌────────────────────────┐
│ AudioInput   │──────────────▶│ ModeManager  │────────────▶│ Leds   │   │ WifiSetup  WebUi  Ota  │
│ FilterBank   │  (queue,      │  active Mode │             │FastLED │   │ MatterLight            │
│ LevelTracker │   newest wins)└──────▲───────┘             └────────┘   └───────────┬────────────┘
└──────────────┘                      │ Settings (mode, colours, brightness, presets) │
                                      └───────────────────────────────────────────────┘
```

- **Audio (core 0):** reads the mic, runs the filter bank, and publishes 150 band levels about
  every 10 ms. It never touches LEDs or the network.
- **LEDs (core 1):** the active mode turns the latest levels (or the time, for lighting modes)
  into 150 colours, then FastLED sends them to the strip.
- **Settings:** one shared object holding the current mode, colours, brightness and presets.
  The web page and Matter change it; modes read it. Changes are saved to flash after a few
  quiet seconds.

### Source layout

Arduino IDE sketch, with everything except the entry point under `src/` (the IDE compiles
`src/` recursively):

```
firmware/SpectrumAnalyzer/
  SpectrumAnalyzer.ino      setup()/loop(): wires the parts together, nothing else
  src/config/               pins, strip length, power limits, tuning constants
  src/audio/                AudioInput (I2S), FilterBank, LevelTracker (dB, noise floor, AGC)
  src/modes/                Mode interface + one file per mode
  src/leds/                 FastLED output and current limit
  src/settings/             Settings model, presets, flash persistence
  src/net/                  WifiSetup, WebUi (+ page as a header), Ota, MatterLight
test/                       PC-side unit tests for audio and mode logic (g++)
docs/                       this document, wiring notes
```

Rules: each module does one job behind a small interface. The audio and mode code is plain
C++ with no Arduino headers, so it is tested on a PC. Hardware code stays thin.

### Libraries

| Library | Why |
|---|---|
| Arduino-ESP32 core 3.x | ESP-IDF 5 I2S driver, Preferences, WebServer, mDNS, Update, Matter |
| FastLED 3.9+ | LED output on the RMT peripheral, current limiting, colour maths |
| WiFiManager | Captive-portal Wi-Fi setup *(pending check against core 3.x and Matter)* |

---

## 4. Audio pipeline

1. **Input:** I2S at 48 kHz, 24-bit samples in 32-bit slots, left channel; 512-sample blocks.
2. **Filter bank:** 150 band-pass filters, log-spaced centres from 40 Hz to 15 kHz (about 17.5
   per octave). Each is 4th order (two biquads). The audio is low-passed and halved in sample
   rate once per octave going down, so low bands run at low rates. Bass bands have a minimum
   bandwidth (default 5 Hz) to keep them responsive. Already prototyped and tested on a PC:
   every band peaks at its own frequency, ±5 neighbours are 33 dB down, no aliasing.
3. **Level:** smoothed power per band → dB, plus a treble boost (default +3 dB/octave about
   1 kHz) because music has less energy up there.
4. **Noise floor:** a per-band floor tracks the quietest recent level slowly, so room noise
   (about −75 dBFS here) stays dark without a startup calibration step.
5. **Automatic gain:** the brightest band sets the top of a 45 dB window, which follows the
   room's loudness: quick to come down when music gets loud, slow to recover.
6. **Silence:** if every band is under its floor for about 1.5 s, spectrum modes fade to dark
   over about 2 s. Any sound brings them back immediately.

CPU use is reported over serial and on the web page, so it can be checked on the hardware.

---

## 5. Modes

All modes take the global brightness and respect the current limit.

| Mode | Behaviour |
|---|---|
| **Spectrum Snappy** | Fast attack, fast fall. **Peak flash:** a band reaching a new peak flashes toward white, then fades back to the strip colour. |
| **Spectrum Smooth** | Softer attack, slow fall. **Peak hold:** each LED holds its peak briefly before falling. |
| **Solid colour** | Every LED one chosen colour. |
| **Gradient** | Blend from colour A at the left to colour B at the right. |
| **Rainbow** | The whole strip in one colour, slowly cycling through the rainbow. |
| **Twinkle** | Random LEDs fade in and out in the chosen colour. |

Spectrum colour: the whole strip is one hue that cycles slowly through the rainbow (default
one full cycle per minute). Each LED's brightness is its band level.

Attack, fall, hold and flash times are tuning constants in `src/config/`, set on the hardware.

---

## 6. Settings and presets

- **Current settings:** mode, colour A, colour B, brightness, on/off.
- **Presets:** 8 slots, each a snapshot of mode, colours and brightness, saved and recalled
  from the web page.
- **Persistence:** stored with Preferences (flash). Saved about 5 s after the last change, to
  limit flash wear. Restored at power-on, so it comes back in its last mode.

---

## 7. Network

### Wi-Fi setup

- If no Wi-Fi is saved, or the saved network can't be reached for about 2 minutes, the board
  starts its own network **Spectrum-Setup** with a captive setup page: join it on a phone,
  pick the home network, enter the password.
- Ways back into setup without reaching the board in the ceiling:
  - **Forget Wi-Fi** button on the web page.
  - **Power-cycle at the plug three times quickly** (each on-time under about 5 s).

### Web page

- `http://spectrum.local` (mDNS), no password. Some older Android phones don't resolve
  `.local` names; the page also shows its IP address, and a router address reservation helps.
- Controls: mode, colours, brightness, on/off, presets (save, recall), Forget Wi-Fi,
  firmware update, and a status panel (Wi-Fi signal, CPU load, audio level).
- The page is a single HTML file compiled into the firmware, so there's no separate file
  upload step in the Arduino IDE.
- Simple JSON endpoints behind the page (`GET/POST /api/state`, `/api/presets`), which also
  make scripting or a Homebridge plugin possible later.

### Firmware updates

- **From the Arduino IDE** over the network (network port), and **from the web page** by
  uploading a `.bin` exported by the IDE.
- Needs a partition scheme with two app slots large enough for the Matter build *(to check)*.

### Matter

- The strip appears as one Matter **colour light**: on/off, brightness, colour. It is paired
  once in Apple Home and once in Google Home (Matter allows several controllers).
- Turning it on, or changing colour or brightness, from a phone or voice assistant switches
  to Solid colour mode (or adjusts the current mode's colour/brightness, *to decide*).
  Choosing between modes stays on the web page.
- Needs a Matter hub in each ecosystem (Apple: HomePod or Apple TV; Google: Nest speaker or
  display).

---

## 8. Build order

Each step ends with something testable on the bench.

1. **Skeleton:** sketch layout, config, FastLED output with current limit, serial log.
2. **Settings:** model, presets, flash persistence, restore at power-on.
3. **Lighting modes:** Solid, Gradient, Rainbow, Twinkle.
4. **Audio:** I2S input, filter bank, level tracking, CPU measurement on hardware.
5. **Spectrum modes:** Snappy and Smooth, silence fade; tuning on the hardware.
6. **Wi-Fi:** setup portal, mDNS, power-cycle reset.
7. **Web page and API.**
8. **Wi-Fi updates.**
9. **Matter.**
10. **Install:** Perma-Proto board, ceiling wiring, current limit raised to the final value.

---

## 9. Open items and risks

| Item | Plan |
|---|---|
| Matter hubs available? | Confirm with owner before step 9. |
| Matter + WiFiManager on the same Wi-Fi radio | Build a small test sketch early, before committing to either library. |
| Flash: Matter build size with two OTA slots in 8 MB | Check the partition scheme with a test build. |
| Filter-bank CPU load on the S3 | Measure in step 4; fall-backs: 32 kHz sample rate or lower top frequency. |
| What Matter brightness/colour does to spectrum modes | Decide before step 9. |
| Current limit | ~2 A while on bench jumper wires; up to 5 A once the 14 AWG wiring is in. |
