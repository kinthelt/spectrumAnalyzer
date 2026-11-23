# spectrumAnalyzer
A spectrum analyzer using an LED strip and an ESP32.

150 LEDs along the top of a wall, one frequency band per LED (bass on the left, treble on the
right), with brightness following the music picked up by a microphone. It also works as
ordinary room lighting, controlled from a web page and from Apple Home and Google Home.

- **Design and build order:** [docs/DESIGN.md](docs/DESIGN.md)
- **Firmware:** [firmware/SpectrumAnalyzer](firmware/SpectrumAnalyzer)

## Status

Build step 1 of 10: firmware skeleton with LED output and a bench test pattern.

## Building the firmware (Arduino IDE)

1. **Boards Manager:** install **esp32 by Espressif Systems**, version 3.x.
2. **Library Manager:** install **FastLED**, version 3.9 or newer.
3. Open `firmware/SpectrumAnalyzer/SpectrumAnalyzer.ino`.
4. **Tools** menu:

   | Setting | Value |
   |---|---|
   | Board | ESP32S3 Dev Module |
   | Flash Size | 8MB (64Mb) |
   | PSRAM | OPI PSRAM |
   | USB CDC On Boot | Enabled (needed when using the board's native USB port) |

5. Upload, then open Serial Monitor at 115200 baud.

Hardware constants (pins, strip length, current limit) are in
`firmware/SpectrumAnalyzer/src/config/Config.h`.
