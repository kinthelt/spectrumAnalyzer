# PC-side unit tests for the hardware-independent firmware code.
#   make -C test        build and run
#   make -C test clean

SRC_DIR := ../firmware/SpectrumAnalyzer/src
CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O1 -g -I$(SRC_DIR) -I.

FIRMWARE_SOURCES := \
	$(SRC_DIR)/settings/LightSettings.cpp \
	$(SRC_DIR)/settings/SettingsCodec.cpp \
	$(SRC_DIR)/settings/SettingsManager.cpp \
	$(SRC_DIR)/util/Rgb.cpp \
	$(SRC_DIR)/modes/ModeManager.cpp \
	$(SRC_DIR)/modes/TwinkleMode.cpp \
	$(SRC_DIR)/modes/SpectrumPlaceholderMode.cpp \
	$(SRC_DIR)/audio/FilterBank.cpp \
	$(SRC_DIR)/audio/LevelTracker.cpp

TEST_SOURCES := test_main.cpp test_settings.cpp test_modes.cpp test_audio.cpp

BUILD := build
BIN := $(BUILD)/run_tests

.PHONY: all clean
all: $(BIN)
	./$(BIN)

$(BIN): $(FIRMWARE_SOURCES) $(TEST_SOURCES) $(wildcard *.h) $(wildcard $(SRC_DIR)/*/*.h)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(FIRMWARE_SOURCES) $(TEST_SOURCES) -o $@

clean:
	rm -rf $(BUILD)
