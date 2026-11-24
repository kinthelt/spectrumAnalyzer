#!/usr/bin/env bash
# Build, upload and monitor the firmware with arduino-cli, using the same board
# settings as the Arduino IDE's Tools menu (kept here, in one place).
#
#   tools/build.sh compile               compile only
#   tools/build.sh upload  [PORT]        compile, then upload
#   tools/build.sh monitor [PORT]        open the serial monitor (Ctrl+C to quit)
#   tools/build.sh flash   [PORT]        compile, upload, then open the monitor
#   tools/build.sh erase   [PORT]        compile, ERASE ALL FLASH, upload
#                                        (wipes settings, presets, Wi-Fi and pairing)
#   tools/build.sh test                  run the PC unit tests
#   tools/build.sh ports                 list connected boards
#
# PORT defaults to the first /dev/ttyACM* or /dev/ttyUSB* found.
# DRY_RUN=1 prints the commands instead of running them.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH="$REPO/firmware/SpectrumAnalyzer"
BAUD=115200

# Tools-menu settings for the Freenove ESP32-S3-WROOM (N8R8). Each name and
# value is checked against the esp32 core 3.3.13 board definitions.
BOARD_OPTIONS=(
  PSRAM=opi                    # PSRAM: OPI PSRAM
  FlashSize=8M                 # Flash Size: 8MB (64Mb)
  PartitionScheme=matter_8MB   # Partition Scheme: Matter 8MB with spiffs
  FlashMode=qio                # Flash Mode: QIO 80MHz
  CDCOnBoot=default            # USB CDC On Boot: Disabled (USB-serial port)
  USBMode=hwcdc                # USB Mode: Hardware CDC and JTAG
  CPUFreq=240                  # CPU Frequency: 240MHz (WiFi)
  UploadSpeed=921600           # Upload Speed: 921600
)

fqbn() {   # $1: EraseFlash value (none or all)
  local IFS=,
  echo "esp32:esp32:esp32s3:${BOARD_OPTIONS[*]},EraseFlash=$1"
}

run() {
  echo "+ $*" >&2
  if [[ "${DRY_RUN:-0}" != 1 ]]; then "$@"; fi
}

need_cli() {
  if ! command -v arduino-cli >/dev/null 2>&1; then
    echo "arduino-cli not found. Install it with:" >&2
    echo "  mkdir -p ~/bin && curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/bin sh" >&2
    echo "then open a new terminal." >&2
    exit 1
  fi
}

find_port() {   # $1: port given on the command line, or empty
  if [[ -n "${1:-}" ]]; then echo "$1"; return; fi
  local p
  for p in /dev/ttyACM* /dev/ttyUSB*; do
    if [[ -e "$p" ]]; then echo "$p"; return; fi
  done
  echo "No board found on /dev/ttyACM* or /dev/ttyUSB*. Is it plugged in?" >&2
  echo "Run 'tools/build.sh ports' to list ports, or give one: tools/build.sh upload /dev/ttyUSB0" >&2
  exit 1
}

compile() {   # $1: EraseFlash value
  need_cli
  run arduino-cli compile --fqbn "$(fqbn "$1")" "$SKETCH"
}

upload() {   # $1: EraseFlash value, $2: port
  need_cli
  run arduino-cli upload -p "$2" --fqbn "$(fqbn "$1")" "$SKETCH"
}

monitor() {   # $1: port
  need_cli
  run arduino-cli monitor -p "$1" -c baudrate="$BAUD"
}

usage() {
  sed -n '2,15p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

cmd="${1:-}"
port_arg="${2:-}"
case "$cmd" in
  compile)
    compile none
    ;;
  upload)
    port="$(find_port "$port_arg")"
    compile none
    upload none "$port"
    ;;
  monitor)
    monitor "$(find_port "$port_arg")"
    ;;
  flash)
    port="$(find_port "$port_arg")"
    compile none
    upload none "$port"
    monitor "$port"
    ;;
  erase)
    port="$(find_port "$port_arg")"
    read -r -p "Erase ALL flash on $port (settings, presets, Wi-Fi, Matter pairing)? [y/N] " answer
    if [[ "$answer" != y && "$answer" != Y ]]; then
      echo "Cancelled." >&2
      exit 1
    fi
    compile all
    upload all "$port"
    ;;
  test)
    run make -C "$REPO/test"
    ;;
  ports)
    need_cli
    run arduino-cli board list
    ;;
  -h | --help | help | "")
    usage
    ;;
  *)
    echo "Unknown command '$cmd'." >&2
    usage >&2
    exit 1
    ;;
esac
