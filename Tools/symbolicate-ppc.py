#!/usr/bin/python

# This filter replace all occurences of JIT_PPC_${address} by a
# corresponding function name JIT_PPC_${symbol} as defined by a .map file.
# TODO, add an option to append the block address (JIT_PPC_${symbol}@${addr})

# Example 1: guest function profiling (excluding host callees)
#
# $ perf record -t $tid
# $ perf script | sed 's/.*cycles: *[0-9a-f]* *//' |
#    python Tools/symbolicate-ppc.py ~/.dolphin-emu/Maps/${map}.map |
#    rankor -r | head
#  10.05% JIT_Loop (/tmp/perf-15936.map)
#   3.73% [unknown] (/tmp/perf-15936.map)
#   1.91% VideoBackendHardware::Video_GatherPipeBursted (/opt/dolphin-2015-05-06/bin/dolphin-emu)
#   1.39% JIT_PPC_PSMTXConcat (/tmp/perf-15936.map)
#   1.00% JIT_PPC_zz_051754c_ (/tmp/perf-15936.map)
#   0.90% JIT_PPC_zz_051751c_ (/tmp/perf-15936.map)
#   0.71% JIT_PPC_zz_04339d4_ (/tmp/perf-15936.map)
#   0.59% JIT_PPC_zz_05173e0_ (/tmp/perf-15936.map)
#   0.57% JIT_PPC_zz_044141c_ (/tmp/perf-15936.map)
#   0.54% JIT_PPC_zz_01839cc_ (/tmp/perf-15936.map)

# Example 2: guest function profiling (including host callees)
#
# $ perf record --call-graph dwarf -t $tid
# $ perf script | stackcollapse-perf.pl | sed 's/^CPU;//' |
#     python Tools/symbolicate-ppc.py ~/.dolphin-emu/Maps/${map}.map |
#     perl -pe 's/^([^; ]*).*? ([0-9]+?)$/\1 \2/' | stackcollapse-recursive.pl |
#     awk '{printf "%s %s\n", $2, $1}' | sort -rn | head
# 5811 JIT_Loop
# 2396 [unknown]
# 577 JIT_PPC_PSMTXConcat
# 464 JIT_PPC___restore_gpr
# 396 JIT_PPC_zz_0517514_
# 313 JIT_PPC_zz_04339d4_
# 290 JIT_PPC_zz_05173e0_
# 285 JIT_PPC_zz_01839cc_
# 277 JIT_PPC_zz_04335ac_
# 269 JIT_PPC_zz_0420b58_

import re
import sys

stdin = sys.stdin
stdout = sys.stdout

class Symbol:
    def __init__(self, start, size, name):
        self.start = start
        self.end = start + size
        self.name = name

# Read a .map file: this is a line-oriented file containing mapping from
# the (PowerPC) memory addresses to function names.
# The format is: "%08x %08x %08x %i %s" (address, size, address, 0, name).
# They should be already be sorted.
def read_map(filename):
    reg = re.compile("^([0-9a-f]{8}) ([0-9a-f]{8}) ([0-9a-f]{8}) ([0-9]*) (.*)$")
    res = []
    with open(filename, "r") as f:
        for line in f:
            match = reg.match(line)
            if match:
                start = int(match.group(1), 16)
                size  = int(match.group(2), 16)
                name  = match.group(5)
                res.append(Symbol(start, size, name))
    return res

map = read_map(sys.argv[1])

# Do a binary each in the map file in order to find the symbol:
def lookup(address):
    i = 0
    j = len(map)
    while(True):
        if (j < i):
            return "JIT_PPC_[unknown]"
        k = round((j + i) // 2)
        if (address < map[k].start):
            j = k - 1
        elif (address >= map[k].end):
            i = k + 1
        else:
            return "JIT_PPC_" + map[k].name

# Function used to replace given match:
def replace(match):
    return lookup(int(match.group(1), 16))

# Process stdin and write to stdout:
for line in stdin:
    modline = re.sub('JIT_PPC_([0-9a-f]*)', replace, line)
    stdout.write(modline)
