# spectrumAnalyzer
A spectrum analyzer using an LED strip and an ESP32.

150 LEDs along the top of a wall, one frequency band per LED (bass on the left, treble on the
right), with brightness following the music picked up by a microphone. It also works as
ordinary room lighting, controlled from a web page and from Apple Home and Google Home.

- **Design and build order:** [docs/DESIGN.md](docs/DESIGN.md)
- **Installation checklist:** [docs/INSTALL.md](docs/INSTALL.md)
- **Firmware:** [firmware/SpectrumAnalyzer](firmware/SpectrumAnalyzer)

## Status

Build step 10 of 10: installation. On first start the board opens a
**Spectrum-Setup** network for entering your Wi-Fi details; after that the web page is at
**http://spectrum.local** (or the board's IP address, shown under Share on the page), and new
firmware can be installed from its Settings page or with `tools/build.sh ota`. Pairing with
Apple Home or Google Home is on the same Settings page. Modes: Spectrum Snappy, Spectrum Smooth, Solid, Gradient, Rainbow, Twinkle;
settings and 8 presets saved to flash; Serial Monitor commands (type `help`).

### Strip length and bass end

Set on the web page under **Settings → Strip setup** (no reflashing needed):

- **Number of LEDs** (1-150), for a strip cut to fit the room. Each LED in use shows one
  frequency band, from 40 Hz at the bass end to 15 kHz. 150 (the full 5 m strip) is the
  limit on today's ESP32-S3: with Apple/Google Home (Matter) running, the board's fast
  internal memory is too tight for more. Changing it restarts the lights.
- **Bass end** (left or right), for a strip mounted the other way round.

"Reset settings" keeps these, like Wi-Fi.

## Building from the command line

`tools/build.sh` wraps [arduino-cli](https://arduino.github.io/arduino-cli/) with the board
settings below, so they live in one place. arduino-cli uses the same `~/.arduino15` board
package and libraries as the Arduino IDE. One-time install:

```bash
mkdir -p ~/bin && curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/bin sh
```

| Command | Does |
|---|---|
| `tools/build.sh compile` | Compile only |
| `tools/build.sh upload [PORT]` | Compile and upload |
| `tools/build.sh flash [PORT]` | Compile, upload, then open the serial monitor (Ctrl+C quits) |
| `tools/build.sh monitor [PORT]` | Serial monitor only |
| `tools/build.sh erase [PORT]` | Compile and upload with **all flash erased** (asks first) |
| `tools/build.sh ota [ADDRESS]` | Compile and install over Wi-Fi (default `spectrum.local`; give the IP if `.local` doesn't resolve). Waits for the restart and prints the new version. |
| `tools/build.sh test` | Run the PC unit tests |
| `tools/build.sh ports` | List connected boards |

PORT defaults to the first `/dev/ttyACM*` or `/dev/ttyUSB*`. Close the IDE's Serial Monitor
before uploading: only one program can use the port.

### Updating over Wi-Fi

Two ways, both through the same checks on the board:

- `tools/build.sh ota` (compiles first), or
- in the Arduino IDE, **Sketch → Export Compiled Binary**, then on the web page
  **Settings → Firmware update → Choose firmware file** and pick
  **`SpectrumAnalyzer.ino.esp32s3.bin`** from the sketch folder. (The export also puts other
  files in the sketch's `build/` folder; the `.merged.bin`, bootloader and partition files are
  refused with a message saying which file to choose.)

The strip turns a steady dim blue while the file arrives (the phone shows the progress), then
flashes green and restarts (red flashes if the update was refused; the old firmware stays). New firmware is kept once it has been on Wi-Fi with the web
page running for 30 s in a row; if that doesn't happen within 5 minutes, or it crashes first,
the previous firmware comes back by itself. USB uploads are unaffected.

## Building the firmware (Arduino IDE)

1. **Boards Manager:** install **esp32 by Espressif Systems**, version **3.3.x**. Don't
   update to 4.x yet: 4.0 moves to ESP-IDF 6.1 and hasn't been tested with this project
   or FastLED.
2. **Library Manager:** install **FastLED** (3.9 or newer) and **WiFiManager** by tzapu.
3. Open `firmware/SpectrumAnalyzer/SpectrumAnalyzer.ino`.
4. **Tools** menu:

   | Setting | Value |
   |---|---|
   | Board | ESP32S3 Dev Module |
   | Flash Size | 8MB (64Mb) |
   | PSRAM | OPI PSRAM |
   | Partition Scheme | **Matter 8MB with spiffs** (room for Matter and over-the-air updates) |
   | USB CDC On Boot | Enabled (needed when using the board's native USB port) |

5. Upload, then open Serial Monitor at 115200 baud with line ending set to "Newline".

**Wi-Fi setup:** on first start, join the **Spectrum-Setup** network on a phone and enter your
Wi-Fi details on the page that opens. The ESP32 needs a **2.4 GHz** network.

**Getting back into Wi-Fi setup** (new router or password), once the board is out of reach:

- **Forget Wi-Fi** on the web page (Settings), if the page still works. This erases the saved
  network and opens **Spectrum-Setup**.
- **Automatically:** if the saved network can't be reached for 2 minutes after power-on, or is
  lost for 5 minutes while running, **Spectrum-Setup** opens by itself.
- **The power-cycle trick**, a last resort when nothing else works: switch off at the plug
  and on again three times in a row, leaving it **on for about 2 seconds each time, never 5
  or more**:

  **on ~2 s → off → on ~2 s → off → on** (leave it on)

  Each power-up only counts once the board has got a little way into starting (about 0.7 s);
  staying on for 5 s clears the count. How long it stays *off* doesn't matter. On the third
  power-up **Spectrum-Setup** opens. The saved network is kept: if nobody uses the setup page,
  after 10 minutes the lights close it and rejoin the home Wi-Fi by themselves. (Serial
  Monitor's `forget` command also works, with USB connected.)

Last tested with esp32 core 3.3.13 and FastLED 3.10.10.

**If linking fails** with undefined references inside the core (`btInUse`,
`setCpuFrequencyMhz`, ...), usually after updating the board package, the cached core
build is stale. Close the IDE, delete the cache, and rebuild:

```bash
rm -rf /tmp/arduino_cache_* /tmp/arduino_build_* ~/.cache/arduino
```

After a board-package update, also re-check the Tools menu: updates can reset Flash Size.

Hardware constants (pins, maximum strip length, current limit) are in
`firmware/SpectrumAnalyzer/src/config/Config.h`.

## Running the PC unit tests

The hardware-independent code (settings now, audio and modes later) has unit tests that run
on a PC with g++ and make:

```bash
make -C test
```

This also checks that `WebPage.h` matches `web/` (see below).

## Working on the web page

The page is `web/index.html` (one file; the QR-code library in `web/vendor/` is put inside
it at build time). The firmware serves a gzipped copy from
`firmware/SpectrumAnalyzer/src/web/WebPage.h`, which is **generated**: after changing
anything in `web/`, run

```bash
python3 tools/embed_web.py
```

and commit the new `WebPage.h` with your change. The Arduino IDE builds from that header.

To try the page without the board, run a stand-in server that answers the same API from
memory, then open http://localhost:8080 (or this computer's address from a phone):

```bash
python3 tools/mock_server.py
```

Browser tests (Playwright with Chromium; phone-sized screens, screenshots in
`test/build/web/`):

```bash
node test/web/page_test.js
```
