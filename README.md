# spectrumAnalyzer
A spectrum analyzer using an LED strip and an ESP32.

150 LEDs along the top of a wall, one frequency band per LED (bass on the left, treble on the
right), with brightness following the music picked up by a microphone. It also works as
ordinary room lighting, controlled from a web page and from Apple Home and Google Home.

- **Design and build order:** [docs/DESIGN.md](docs/DESIGN.md)
- **Firmware:** [firmware/SpectrumAnalyzer](firmware/SpectrumAnalyzer)

## Status

Build step 3 of 10: lighting modes Solid, Gradient, Rainbow and Twinkle. Settings and 8
presets are saved to flash and changed from Serial Monitor (type `help`). The two spectrum
modes show a placeholder comet until the audio pipeline is built (steps 4 and 5).

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

5. Upload, then open Serial Monitor at 115200 baud with line ending set to "Newline".

Last tested with esp32 core 3.3.11 and FastLED 3.10.10.

Hardware constants (pins, strip length, current limit) are in
`firmware/SpectrumAnalyzer/src/config/Config.h`.

## Running the PC unit tests

The hardware-independent code (settings now, audio and modes later) has unit tests that run
on a PC with g++ and make:

```bash
make -C test
```
