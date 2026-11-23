#include "SerialConsole.h"

#include <Arduino.h>

#include <cstdlib>
#include <cstring>

void SerialConsole::poll() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      line_[length_] = '\0';
      if (length_ > 0) runLine(line_);
      length_ = 0;
    } else if (length_ < kMaxLine - 1) {
      line_[length_++] = c;
    }
  }
}

bool SerialConsole::parseSlot(const char* text, int* slot) {
  if (text == nullptr) return false;
  const int n = std::atoi(text);
  if (n < 1 || n > SettingsManager::kPresetCount) return false;
  *slot = n - 1;
  return true;
}

void SerialConsole::printHelp() {
  Serial.println(
      "Commands:\n"
      "  status                 show current settings and presets\n"
      "  on | off               turn the strip on or off\n"
      "  bright <0-255>         set brightness\n"
      "  mode <name>            spectrum-snappy, spectrum-smooth, solid, gradient,\n"
      "                         rainbow, twinkle (only stored until step 3)\n"
      "  colour a|b <RRGGBB>    set colour A or B, e.g. colour a FF8000\n"
      "  spectrum rainbow|fixed spectrum colouring\n"
      "  save <1-8>             save current settings as a preset\n"
      "  recall <1-8>           load a preset\n"
      "  delete <1-8>           delete a preset\n"
      "  reset                  erase everything (asks to confirm)");
}

void SerialConsole::printStatus() {
  const LightSettings& s = settings_.current();
  char a[7], b[7];
  formatHexColour(s.colourA, a);
  formatHexColour(s.colourB, b);
  Serial.printf("%s, mode %s, brightness %u, colour A %s, colour B %s, spectrum %s%s\n",
                s.on ? "ON" : "OFF", modeName(s.mode), s.brightness, a, b,
                spectrumColourName(s.spectrumColour),
                settings_.hasUnsavedChanges() ? " (not saved yet)" : "");
  for (int slot = 0; slot < SettingsManager::kPresetCount; ++slot) {
    const LightSettings* p = settings_.preset(slot);
    if (p == nullptr) {
      Serial.printf("  preset %d: empty\n", slot + 1);
      continue;
    }
    formatHexColour(p->colourA, a);
    formatHexColour(p->colourB, b);
    Serial.printf("  preset %d: %s, brightness %u, A %s, B %s, spectrum %s\n", slot + 1,
                  modeName(p->mode), p->brightness, a, b, spectrumColourName(p->spectrumColour));
  }
}

void SerialConsole::runLine(char* line) {
  const char* cmd = std::strtok(line, " ");
  const char* arg1 = std::strtok(nullptr, " ");
  const char* arg2 = std::strtok(nullptr, " ");
  const uint32_t now = millis();
  if (cmd == nullptr) return;

  const bool wasArmed = resetArmed_;
  resetArmed_ = false;

  if (std::strcmp(cmd, "help") == 0) {
    printHelp();
  } else if (std::strcmp(cmd, "status") == 0) {
    printStatus();
  } else if (std::strcmp(cmd, "on") == 0) {
    settings_.setOn(true, now);
    printStatus();
  } else if (std::strcmp(cmd, "off") == 0) {
    settings_.setOn(false, now);
    printStatus();
  } else if (std::strcmp(cmd, "bright") == 0 && arg1 != nullptr) {
    const int value = std::atoi(arg1);
    if (value < 0 || value > 255) {
      Serial.println("Brightness must be 0-255");
      return;
    }
    settings_.setBrightness(static_cast<uint8_t>(value), now);
    printStatus();
  } else if (std::strcmp(cmd, "mode") == 0 && arg1 != nullptr) {
    Mode mode;
    if (!modeFromName(arg1, &mode)) {
      Serial.println("Unknown mode. Type 'help' for the list.");
      return;
    }
    settings_.setMode(mode, now);
    printStatus();
  } else if (std::strcmp(cmd, "colour") == 0 && arg1 != nullptr && arg2 != nullptr) {
    Rgb colour;
    if (!parseHexColour(arg2, &colour)) {
      Serial.println("Colour must be 6 hex digits, e.g. FF8000");
      return;
    }
    if (std::strcmp(arg1, "a") == 0) {
      settings_.setColourA(colour, now);
    } else if (std::strcmp(arg1, "b") == 0) {
      settings_.setColourB(colour, now);
    } else {
      Serial.println("Use 'colour a <RRGGBB>' or 'colour b <RRGGBB>'");
      return;
    }
    printStatus();
  } else if (std::strcmp(cmd, "spectrum") == 0 && arg1 != nullptr) {
    SpectrumColour sc;
    if (!spectrumColourFromName(arg1, &sc)) {
      Serial.println("Use 'spectrum rainbow' or 'spectrum fixed'");
      return;
    }
    settings_.setSpectrumColour(sc, now);
    printStatus();
  } else if (std::strcmp(cmd, "save") == 0 || std::strcmp(cmd, "recall") == 0 ||
             std::strcmp(cmd, "delete") == 0) {
    int slot;
    if (!parseSlot(arg1, &slot)) {
      Serial.printf("Give a preset number 1-%d\n", SettingsManager::kPresetCount);
      return;
    }
    bool ok;
    if (cmd[0] == 's') {
      ok = settings_.savePreset(slot);
    } else if (cmd[0] == 'r') {
      ok = settings_.recallPreset(slot, now);
    } else {
      ok = settings_.deletePreset(slot);
    }
    if (!ok) Serial.printf("Preset %d: %s failed (empty slot?)\n", slot + 1, cmd);
    printStatus();
  } else if (std::strcmp(cmd, "reset") == 0) {
    if (arg1 != nullptr && std::strcmp(arg1, "yes") == 0 && wasArmed) {
      settings_.factoryReset();
      Serial.println("Everything erased. Back to defaults.");
      printStatus();
    } else {
      resetArmed_ = true;
      Serial.println("This erases all settings and presets. Type 'reset yes' to confirm.");
    }
  } else {
    Serial.println("Unknown command. Type 'help'.");
  }
}
