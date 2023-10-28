#!/bin/bash
#
# Using this script instead of objdump enables perf to disassemble
# and annotate any JIT code (given a symbol file).
#
# To run perf without root:
#   kernel.perf_event_paranoid = -1
# To trace a process without root:
#   kernel.yama.ptrace_scope = 0
#
# Example usage:
# $ dolphin-emu -C Dolphin.Core.PerfMapDir=/tmp -b -e $game
# $ perf top -p $(pidof dolphin-emu) --objdump ./Tools/perf-disassemble.sh -M intel

flavor=att
raw=r
src=

[[ "${@: -1}" != /tmp/perf-*.map ]] && { objdump "$@"; exit; }

while [ "$1" ]
do
    case "$1" in
        -M)
            flavor=$2
            shift
            ;;
        --start-address=*)
            start="${1##--start-address=}"
            ;;
        --stop-address=*)
            stop="${1##--stop-address=}"
            ;;
        --no-show-raw-insn)
            raw=
            ;;
        --no-show-raw)
            raw=
            ;;
        -S)
            src=m
            ;;
        -[ldC])
            ;;
        -*)
            echo "Unknown parameter '$1'"
            ;;
        *)
            ;;
    esac
    shift
done
gdb -q -p $(pidof dolphin-emu) -ex "set disassembly $flavor" -ex "disas /$raw$src $start,$stop" -ex q -batch
