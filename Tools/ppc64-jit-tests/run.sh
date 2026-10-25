#!/bin/bash
# Runs a guest program on a CPU core and prints its reports.
# Usage: run.sh <dolphin-emu-nogui> <program.elf> <core number> [timeout seconds] [extra -C options...]
# Cores: 0 = Interpreter, 5 = Cached Interpreter, 6 = JITPPC64.
set -u
dolphin=$1
program=$2
core=$3
timeout=${4:-600}
shift 4 2>/dev/null || shift $#

user=$(mktemp -d)
log=$(mktemp)
trap 'rm -rf "$user" "$log"' EXIT

"$dolphin" -u "$user" -p headless -v Null -e "$program" \
  -C Dolphin.Core.CPUCore="$core" \
  -C Dolphin.DSP.Backend="No Audio Output" \
  -C Logger.Options.WriteToConsole=True \
  -C Logger.Options.Verbosity=2 \
  -C Logger.Logs.OSREPORT_HLE=True \
  -C Dolphin.Interface.DebugModeEnabled=True \
  "$@" > "$log" 2>&1 &
pid=$!

# Wait until the program reports that it is done, or crashes, or times out, or floods the log.
for ((i = 0; i < timeout * 10; i++)); do
  if grep -a -q "TEST done" "$log" || ! kill -0 "$pid" 2>/dev/null ||
    (($(stat -c %s "$log") > 100000000)); then
    break
  fi
  sleep 0.1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

# Keep the program's reports, without timestamps and log prefixes.
grep -a -o -E "(START|TEST .*|exceptions: .*)$" "$log" || {
  echo "No reports; log:"
  tail -20 "$log"
}
