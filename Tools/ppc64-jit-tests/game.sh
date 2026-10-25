#!/bin/bash
# Runs a game on a CPU core for a while, dumping every frame losslessly, and prints an MD5 of each
# frame. Running two cores and comparing the output finds the first frame where they differ.
# Usage: game.sh <dolphin-emu-nogui> <game> <core number> <seconds> [extra -C options...]
# Cores: 0 = Interpreter, 5 = Cached Interpreter, 6 = JITPPC64.
# Needs an X server, e.g. Xvfb :99, for the software renderer.
set -u
dolphin=$1
game=$2
core=$3
seconds=$4
shift 4

user=$(mktemp -d)
trap 'rm -rf "$user"' EXIT

# Graphics settings aren't taken from -C, so write them: dump frames losslessly.
mkdir -p "$user/Config"
printf '[Settings]\nDumpCodec = ffv1\n' > "$user/Config/GFX.ini"

DISPLAY=${DISPLAY:-:99} timeout "$seconds" "$dolphin" -u "$user" -p x11 -v "Software Renderer" -e "$game" \
  -C Dolphin.Core.CPUCore="$core" \
  -C Dolphin.Core.CPUThread=False \
  -C Dolphin.Core.EmulationSpeed=0 \
  -C Dolphin.Core.EnableCustomRTC=True \
  -C Dolphin.Core.CustomRTCValue=0x386D4380 \
  -C Dolphin.DSP.Backend="No Audio Output" \
  -C Dolphin.Movie.DumpFrames=True \
  "$@" > "$user/log.txt" 2>&1

video=$(ls "$user"/Dump/Frames/*.avi 2>/dev/null | head -1)
if [ -z "$video" ]; then
  echo "No frames dumped; log:" >&2
  tail -20 "$user/log.txt" >&2
  exit 1
fi
# One line per frame: frame number and MD5 of its pixels. The last frame may be cut off by the
# timeout, so leave it out.
ffmpeg -v error -i "$video" -f framemd5 - | grep -v "^#" | awk -F, '{gsub(/ /, "", $6); print NR - 1, $6}' |
  head -n -1
