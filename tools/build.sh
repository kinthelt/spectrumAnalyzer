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
#   tools/build.sh ota     [ADDRESS]     compile, then update the lights over Wi-Fi
#                                        (ADDRESS defaults to spectrum.local; use the
#                                        IP address if this computer can't find .local)
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

usage() {   # the comment block at the top of this file
  awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "${BASH_SOURCE[0]}"
}

json_field() {   # $1: Python expression on d, the JSON read from stdin
  python3 -c "import json, sys; d = json.load(sys.stdin); print($1)"
}

# Sends a firmware file to the lights' /api/update (the same as the web page's
# Settings > Firmware update), then waits for them to restart and reports the version.
send_update() {   # $1: .bin file, $2: address
  local bin="$1" host="$2" url="http://$2"
  for tool in curl python3; do
    command -v "$tool" >/dev/null 2>&1 || { echo "$tool is needed for 'ota'." >&2; exit 1; }
  done
  if [[ "${DRY_RUN:-0}" == 1 ]]; then
    echo "+ curl --fail-with-body -F firmware=@$bin $url/api/update" >&2
    return
  fi
  [[ -f "$bin" ]] || { echo "No $bin after compiling." >&2; exit 1; }
  local before
  if ! before="$(curl -sS --max-time 5 "$url/api/state" | json_field 'd["status"]["version"]')"; then
    echo "Can't reach the lights at $url. Are they on? If this computer can't find" >&2
    echo ".local names, give the IP address (Settings on the web page shows it):" >&2
    echo "  tools/build.sh ota 192.168.x.x" >&2
    exit 1
  fi
  echo "Lights at $url are on version $before. Sending $(basename "$bin") ($(du -h "$bin" | cut -f1))..." >&2
  local reply
  reply="$(mktemp)"
  if ! curl --fail-with-body --progress-bar -o "$reply" \
       -F "firmware=@$bin;type=application/octet-stream" "$url/api/update"; then
    echo "The lights refused the update: $(json_field 'd.get("error", d)' <"$reply" 2>/dev/null || cat "$reply")" >&2
    rm -f "$reply"
    exit 1
  fi
  rm -f "$reply"
  echo "Installed. Waiting for the lights to restart..." >&2
  sleep 3   # they restart 1.5 s after replying; anything answering after this is the new firmware
  local version="" i
  for i in $(seq 1 60); do
    if version="$(curl -sS --max-time 3 "$url/api/state" 2>/dev/null | json_field 'd["status"]["version"]' 2>/dev/null)"; then
      break
    fi
    version=""
    sleep 2
  done
  if [[ -z "$version" ]]; then
    echo "The lights haven't answered after 2 minutes. If the new firmware can't get on Wi-Fi," >&2
    echo "the previous one returns by itself within 5 minutes." >&2
    exit 1
  fi
  echo "Lights are back, running version $version. It stays once it has been on Wi-Fi for 30 s" >&2
  echo "(the serial log says 'is good: it stays')." >&2
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
  ota)
    need_cli
    out="$REPO/build/ota"   # build/ is ignored by git
    run arduino-cli compile --fqbn "$(fqbn none)" --output-dir "$out" "$SKETCH"
    send_update "$out/SpectrumAnalyzer.ino.bin" "${port_arg:-spectrum.local}"
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
