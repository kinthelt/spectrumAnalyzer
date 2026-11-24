# spectrumAnalyzer
A spectrum analyzer using an LED strip and an ESP32.

150 LEDs along the top of a wall, one frequency band per LED (bass on the left, treble on the
right), with brightness following the music picked up by a microphone. It also works as
ordinary room lighting, controlled from a web page and from Apple Home and Google Home.

- **Design and build order:** [docs/DESIGN.md](docs/DESIGN.md)
- **Firmware:** [firmware/SpectrumAnalyzer](firmware/SpectrumAnalyzer)

## Status

Build step 5 of 10: the spectrum display works. Modes: Spectrum Snappy, Spectrum Smooth,
Solid, Gradient, Rainbow, Twinkle, with settings and 8 presets saved to flash, all changed
from Serial Monitor for now (type `help`). Next: Wi-Fi and the web page.

## Building the firmware (Arduino IDE)

1. **Boards Manager:** install **esp32 by Espressif Systems**, version **3.3.x**. Don't
   update to 4.x yet: 4.0 moves to ESP-IDF 6.1 and hasn't been tested with this project
   or FastLED.
2. **Library Manager:** install **FastLED**, version 3.9 or newer.
3. Open `firmware/SpectrumAnalyzer/SpectrumAnalyzer.ino`.
4. **Tools** menu:

   | Setting | Value |
   |---|---|
   | Board | ESP32S3 Dev Module |
   | Flash Size | 8MB (64Mb) |
   | PSRAM | OPI PSRAM |
   | USB CDC On Boot | Enabled (needed when using the board's native USB port) |

5. Upload, then open Serial Monitor at 115200 baud with line ending set to "Newline".

Last tested with esp32 core 3.3.13 and FastLED 3.10.10.

**If linking fails** with undefined references inside the core (`btInUse`,
`setCpuFrequencyMhz`, ...), usually after updating the board package, the cached core
build is stale. Close the IDE, delete the cache, and rebuild:

```bash
rm -rf /tmp/arduino_cache_* /tmp/arduino_build_* ~/.cache/arduino
```

After a board-package update, also re-check the Tools menu: updates can reset Flash Size.

Hardware constants (pins, strip length, current limit) are in
`firmware/SpectrumAnalyzer/src/config/Config.h`.

## Running the PC unit tests

The hardware-independent code (settings now, audio and modes later) has unit tests that run
on a PC with g++ and make:

```bash
make -C test
```
