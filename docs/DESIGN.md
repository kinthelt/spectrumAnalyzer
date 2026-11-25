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

| Where | Task | Priority | Does |
|---|---|---|---|
| Core 0 | Audio task | 5 | Mic, filter bank, levels; publishes each frame to a one-slot queue (LED task) and a shared snapshot (everyone else). ~43% of the core. |
| Core 1 | LED task | 3 | Every 10 ms on a fixed beat: newest audio frame, settings snapshot, active mode, FastLED, frame stats. |
| Core 1 | Arduino `loop()` | 1 | Settings (the only writer), serial console, Wi-Fi, web page. Publishes a settings copy when they change. |

- The LED task outranks `loop()`, so web requests or the Wi-Fi setup page never delay a
  frame. When frames were drawn from `loop()`, network work stretched them to 93.7 fps.
- Data crossing between tasks goes only through `Shared<T>` (a value copied in and out
  under a lock) or the audio queue. No task reads another's live objects.
- **Settings:** changed only from `loop()` (serial console, web page, and Matter in
  step 9 via `loop()` too), saved to flash a few quiet seconds after the last change.

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
  src/net/                  Wi-Fi state machine and setup network, spectrum.local, power-cycle trick
  src/web/                  WebApi (what requests mean; PC-tested), WebUi (HTTP glue), WebPage.h
                            (generated)
  src/update/               UpdateSession (upload rules), BootValidator (keep or go back),
                            FirmwareMarker (PC-tested); FirmwareUpdate (Update library, OTA glue)
  src/diag/, src/util/      statistics, serial console; logging, colours, JSON, Shared<T>
web/                        the web page source (index.html, vendor/qrcode.js)
tools/                      build.sh, embed_web.py (page -> WebPage.h), mock_server.py
test/                       PC-side unit tests (g++); test/web/ browser tests (Playwright)
docs/                       this document, wiring notes
```

Rules: each module does one job behind a small interface. The audio and mode code is plain
C++ with no Arduino headers, so it is tested on a PC. Hardware code stays thin.

### Libraries

| Library | Why |
|---|---|
| Arduino-ESP32 core 3.x | ESP-IDF 5 I2S driver, Preferences, WebServer, mDNS, Update, Matter |
| FastLED 3.9+ | LED output on the RMT peripheral, current limiting, colour maths |
| WiFiManager (tzapu) | Captive-portal Wi-Fi setup (checked with core 3.3 and Matter in step 6) |
| qrcode-generator 2.0.4 (MIT) | Share code on the web page; vendored in `web/vendor/`, inlined at build time |

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
4. **Noise floor and gate (no calibration step):**
   - Each band's floor and noise spread are learned **only during silence**, when the room
     can be measured without bias. During sound, floors are frozen, so music is never
     learned as noise. (Letting floors follow dips during sound was tried: noise in narrow
     bands dips deep, floors sank ~6 dB a minute under quiet music, and the room's noise
     lit up afterwards.) A floor that is clearly too high, judged on a 1 s average, drops
     quickly during silence.
   - A band lights only above its gate: floor + 3.5 × its noise spread, at least 6 dB.
     Narrow bass bands get ~15 dB, treble 6 dB.
   - A steady hum lighting only a band or two is learned by itself. A new noise that lights
     many bands (a fan) can't be told apart from steady music, so it's learned on request:
     `learn` in Serial Monitor, later a web-page button.
   - Floors are capped (−78 dB per band) and a half-second warm-up after power-on lets them
     find the room.
   - Known limitation: if music is already playing at power-on, the strip may stay dark
     until the first pause (e.g. between songs), then recovers fully.
5. **Automatic gain:** the loudest band sets the top of a 45 dB window, which follows the
   room's loudness: quick to rise, recovering 3 dB/s. Each band's window is at least 20 dB
   above its gate, so noise alone can't fill it.
6. **Silence:** after power-on it must be earned: 1.5 s with nothing lit (and the room
   plausibly quiet), or at most 2 bands lit during a quiet moment. Once silent, wobbles in
   the room's level don't end it; only more bands lighting or a clearly louder room does.
   Spectrum modes fade to dark over about 2 s; any sound brings them back immediately.

Simulated results (PC tests): quiet room lights an LED in 0.07% of frames; a room whose
level wobbles several dB stays silent 100% of the time; floors move 0.1 dB over 5 minutes
of music; silence returns after 5 minutes of quiet music (real filter bank); music
already playing at power-on shows again after the first pause.

Measured on the hardware (core 3.3.11): analysis takes 4.5 ms of each 10.7 ms block,
**43% of core 0**; 93.6 blocks/s; LEDs unaffected at 99.8 fps.

CPU use is reported over serial and on the web page's Settings view.

---

## 5. Modes

All modes take the global brightness and respect the current limit.

| Mode | Behaviour |
|---|---|
| **Spectrum Snappy** | Instant rise, fast fall (full to dark in ~0.33 s). **Peak flash:** a jump of 0.25+ in one frame to 0.5+ flashes toward white, fading back over 0.25 s. |
| **Spectrum Smooth** | Softened rise (60 ms), **peak hold** 0.4 s, then a slow fall (full to dark in ~1.25 s). |
| **Solid colour** | Every LED one chosen colour. |
| **Gradient** | Blend from colour A at the left to colour B at the right. |
| **Rainbow** | The whole strip in one colour, slowly cycling through the rainbow. |
| **Twinkle** | Random LEDs fade in and out in the chosen colour. |

Spectrum colour: either **rainbow cycle** (default: the whole strip is one hue that cycles
slowly through the rainbow, one full cycle per minute) or **fixed colour** (colour A).
Each LED's brightness is its band level raised to the power 2.2, so equal steps on the
dB scale look equally bright. In silence, spectrum modes fade to dark over 2 s; if audio
frames stop arriving (audio failure), they go dark too.

Attack, fall, hold and flash times are tuning constants in `src/config/`, set on the hardware.

---

## 6. Settings and presets

- **Current settings:** mode, colour A, colour B, spectrum colour (rainbow cycle or fixed),
  brightness, on/off.
- **Presets:** 8 slots, each a snapshot of mode, colours and brightness, saved and recalled
  from the web page.
- **Persistence:** stored with Preferences (flash). Saved about 5 s after the last change, to
  limit flash wear. Restored at power-on, so it comes back in its last mode.

---

## 7. Network

### Wi-Fi setup

Decided after a bench spike (`experiments/WifiMatterSpike`): **setup page plus Matter**
(option A). Wi-Fi is set up independently of Matter, so the web page and updates keep
working even if Matter misbehaves; Matter later pairs over the network
(`Matter.setBLECommissioningEnabled(false)`).

- If no Wi-Fi is saved, the board opens its own network **Spectrum-Setup** with a captive
  setup page (WiFiManager, non-blocking: LEDs and audio keep running). Join it on a phone,
  pick the home network, enter the password.
- Saved network unreachable for 2 minutes after boot, or lost for 5 minutes while running:
  the setup network opens. It closes after 10 minutes and the saved network is tried again
  (the router may simply have been off).
- Ways back into setup without reaching the board in the ceiling:
  - **Forget Wi-Fi** (Settings on the web page, or Serial Monitor `forget`).
  - **Power-cycle at the plug three times quickly** (each on-time under 5 s).

Spike findings that shaped the code:
- Matter starts Wi-Fi through ESP-IDF directly, so Arduino's `WiFi.status()` can read
  "down" while connected. Connection state is read from ESP-IDF (`esp_netif`) instead.
- Matter renames the board's mDNS hostname, which broke `spectrum.local` after a minute.
  `spectrum` is registered as a **delegated** mDNS host, which survives the rename.
- Wi-Fi connecting and Matter pairing stalled core 0 once for ~55 ms (normally ~12 ms);
  the microphone buffer was doubled to ~85 ms to ride it out.
- Matter, Wi-Fi updates and the firmware fit the **"Matter 8MB with spiffs"** partition
  scheme (two 3.25 MB app slots).
- Apple Home pairing works, and survives switching off at the plug.

### Web page

- `http://spectrum.local` (mDNS), no password: anyone on the home Wi-Fi can use every
  control. Mostly used from guests' phones (iPhone and Android), so it is built for small
  touch screens, follows the system's light/dark setting, and has no build framework.
- **Main view:** on/off, brightness (follows the finger live), the six modes, the colour
  controls the current mode uses, and presets. Presets are named; only saved ones are listed,
  with "Save current look as a preset" for the next free slot. Editing a preset renames,
  replaces or deletes it (deleting asks first; Enter in the name box never deletes).
- **Share:** a QR code of the board's IP address (`http://<ip>/`), because some Android phones
  don't resolve `.local` names. A DHCP reservation on the router keeps that address fixed.
- **Settings view:** status (version, Wi-Fi network and signal, address, audio, CPU load,
  uptime), and maintenance actions, each confirmed first: Learn room noise, Forget Wi-Fi,
  Reset settings (Wi-Fi is kept). Above them, Firmware update (see below).
- **Staying in sync:** the page polls `/api/state` every 2 s, so changes from another phone
  (or Apple/Google Home later) appear within a couple of seconds. While a finger is on a
  control, polled values don't fight it. Live controls send at most 10 changes a second,
  one request at a time, always ending on the final value; flash is written once afterwards
  (5 s debounce).
- **Built into the firmware:** `tools/embed_web.py` inlines the QR library, gzips the page
  (about 20 KB) and writes `src/web/WebPage.h`; no file-upload step in the Arduino IDE.
  The page is sent with an ETag, so phones re-download it only after a firmware update.
- **API** (form-encoded POSTs; each answers with the full state, or `{"error": "..."}` and
  HTTP 400; changes are all or nothing):

  | Request | Fields |
  |---|---|
  | `GET /api/state` | everything the page shows, including a `revision` counter and `status` |
  | `POST /api/state` | `on`, `mode`, `brightness` (0-255), `colourA`, `colourB` (`#RRGGBB`), `spectrumColour` (`rainbow`/`fixed`) |
  | `POST /api/preset/save` | `slot` (1-8), `name` (optional; keeps the old name) |
  | `POST /api/preset/recall`, `/delete` | `slot` |
  | `POST /api/preset/rename` | `slot`, `name` (up to 24 bytes of UTF-8) |
  | `POST /api/action/learn-noise`, `/forget-wifi`, `/reset-settings` | none |

  The same API makes scripting or a Homebridge plugin possible later.
- **Testing without the board:** `tools/mock_server.py` serves the built page and answers
  the same API from memory; `test/web/page_test.js` drives it in Chromium at two phone sizes.
  `WebApi` itself is unit-tested on the PC.

### Firmware updates

- **One way in:** `POST /api/update` (multipart form, field `firmware`). The web page's
  Settings → Firmware update and `tools/build.sh ota` (compile, then `curl`) both use it. No
  password, like the rest of the page (decided). No Arduino IDE network port: one less
  library and one less open port.
- **The file:** Arduino IDE 1.8 → Sketch → Export Compiled Binary saves
  `SpectrumAnalyzer.ino.esp32s3.bin` in the sketch folder; the board package also copies every
  build file (including `.merged.bin`) into the sketch's `build/` folder.
- **Checks before the new firmware is used** (`UpdateSession`, unit-tested):
  - fits the spare slot (the "Matter 8MB with spiffs" scheme's 3.25 MB app slots), so the
    whole-flash `.merged.bin` is refused before anything is written;
  - first byte 0xE9 (an ESP32 image);
  - contains this project's marker text (`FirmwareMarker.h`), so another sketch's firmware,
    or the bootloader file, is refused;
  - the Update library's own checks at the end: complete, checksums, built for an ESP32-S3.
  Only after all of them does the bootloader switch to the new slot. The page also catches
  the usual wrong picks by file name before sending.
- **Strip:** one steady dim blue while the file arrives (brightness fixed so it shows even
  if the lights were off); progress is shown on the phone. A live progress bar was tried
  and dropped: writing to flash pauses both cores thousands of times, and strip data sent
  during a pause gets garbled (it flickered). So the LED task sends the blue a few times,
  then stops sending until the upload ends; WS2812s hold their last colour. Green flashes
  when installed, then the board restarts
  into the saved mode and settings (saved first). Red flashes if refused; the old firmware
  carries on.
- **Going back automatically** (`BootValidator`, unit-tested): the esp32 3.3.x board package
  is built with the bootloader's app rollback on (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`).
  The sketch overrides the core's `verifyRollbackLater()` hook (it must be `extern "C"`; a
  plain C++ definition is silently ignored), so new firmware stays on trial until it has
  been on the home Wi-Fi with the web page running for **30 s in a row**. If that hasn't
  happened **5 min** after starting, it goes back; if it crashes or loses power while on
  trial, the bootloader goes back by itself. Settings → Status notes when an update was undone.
  A power cut or router outage during those minutes also means going back: the update is
  simply sent again.
- **Audio analysis pauses during an upload.** Erasing flash freezes core 0 for ~45 ms per
  4 KB, close to half of core 0 at upload speed; with the analysis's 43% and the network
  task, core 0's idle task starved and the task watchdog restarted the board part-way
  (bench). The microphone is still read, so its buffers stay drained; the analysis resumes
  if the update fails, and the board restarts if it succeeds.
- **During an upload** the Arduino loop is busy receiving (the web server handles one request
  at a time), so other phones' pages wait; the LEDs and audio run on their own tasks.

### Matter

- The strip appears as one Matter **colour light**: on/off, brightness, colour. It is paired
  once in Apple Home and once in Google Home (Matter allows several controllers).
- Changes from Apple Home or Google Home **adjust the current mode** and never switch modes:
  - **On/off** turns the strip on or off.
  - **Brightness** sets the global brightness.
  - **Colour** sets the current mode's colour (colour A). In the spectrum modes it replaces
    the rainbow cycle with that fixed colour; choosing the rainbow again is done on the web
    page.
- Choosing between modes stays on the web page.
- Hubs: Apple TV (Apple Home) and Google Nest speaker (Google Home), both confirmed.

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
8. **Wi-Fi updates:** upload checks, update colours on the strip, automatic rollback.
9. **Matter.**
10. **Install:** Perma-Proto board, ceiling wiring, current limit raised to the final value.

---

## 9. Open items and risks

| Item | Plan |
|---|---|
| Matter + WiFiManager on the same Wi-Fi radio | Tested in the spike: works (setup page first, then Matter pairs over Wi-Fi). |
| Flash: Matter build size with two OTA slots in 8 MB | "Matter 8MB with spiffs" scheme: two 3.25 MB app slots. |
| Filter-bank CPU load on the S3 | Measured: 43% of core 0. With Wi-Fi and Matter running, a stand-in kept ~12 ms timing (one 55 ms stall while pairing; mic buffer doubled). |
| Current limit | ~2 A while on bench jumper wires; up to 5 A once the 14 AWG wiring is in. |
| `E cache: esp_cache_msync(113): invalid addr or null pointer` logged once at boot during FastLED start-up (core 3.3.11, FastLED 3.10.10), with or without PSRAM | No visible effect so far. Re-check in step 4 when audio DMA runs alongside the LED driver. |
