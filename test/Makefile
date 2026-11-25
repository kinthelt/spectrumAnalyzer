# PC-side unit tests for the hardware-independent firmware code.
#   make -C test        build and run
#   make -C test clean

SRC_DIR := ../firmware/SpectrumAnalyzer/src
CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O1 -g -pthread -I$(SRC_DIR) -I.

FIRMWARE_SOURCES := \
	$(SRC_DIR)/settings/LightSettings.cpp \
	$(SRC_DIR)/settings/SettingsCodec.cpp \
	$(SRC_DIR)/settings/SettingsManager.cpp \
	$(SRC_DIR)/settings/PresetName.cpp \
	$(SRC_DIR)/util/Rgb.cpp \
	$(SRC_DIR)/modes/ModeManager.cpp \
	$(SRC_DIR)/modes/TwinkleMode.cpp \
	$(SRC_DIR)/modes/SpectrumMode.cpp \
	$(SRC_DIR)/audio/FilterBank.cpp \
	$(SRC_DIR)/audio/LevelTracker.cpp \
	$(SRC_DIR)/net/WifiStateMachine.cpp \
	$(SRC_DIR)/net/PowerCycleCounter.cpp \
	$(SRC_DIR)/util/JsonWriter.cpp \
	$(SRC_DIR)/web/WebApi.cpp \
	$(SRC_DIR)/update/FirmwareMarker.cpp \
	$(SRC_DIR)/update/UpdateSession.cpp \
	$(SRC_DIR)/update/BootValidator.cpp \
	$(SRC_DIR)/modes/UpdateBar.cpp \
	$(SRC_DIR)/smarthome/HomeRules.cpp \
	$(SRC_DIR)/smarthome/CrashGuard.cpp \
	$(SRC_DIR)/settings/StripSetup.cpp \
	$(SRC_DIR)/settings/StripSetupManager.cpp

TEST_SOURCES := test_main.cpp test_settings.cpp test_modes.cpp test_audio.cpp test_net.cpp test_web.cpp test_update.cpp test_home.cpp test_strip.cpp

BUILD := build
BIN := $(BUILD)/run_tests

.PHONY: all clean
all: $(BIN)
	@mkdir -p $(BUILD)/json
	./$(BIN)
	@python3 -c 'import glob, json, sys; files = sorted(glob.glob("$(BUILD)/json/*.json")); [json.load(open(f, encoding="utf-8")) for f in files]; print(f"{len(files)} JSON samples parse as valid JSON")'
	@python3 ../tools/embed_web.py --check

$(BIN): $(FIRMWARE_SOURCES) $(TEST_SOURCES) $(wildcard *.h) $(wildcard $(SRC_DIR)/*/*.h)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(FIRMWARE_SOURCES) $(TEST_SOURCES) -o $@

clean:
	rm -rf $(BUILD)
