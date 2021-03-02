# Copyright 2021 Dolphin Emulator Project
# Licensed under GPLv2+
# Refer to the license.txt file included.

#@category Dolphin

from collections import namedtuple


DolphinSymbol = namedtuple("DolphinSymbol", [
    "section", "addr", "size", "vaddr", "align", "name"
])


def load_dolphin_map(filepath):
    with open(filepath, "r") as f:
        section = ""
        symbol_map = []
        for line in f.readlines():
            t = line.strip().split(" ", 4)
            if len(t) == 3 and t[1] == "section" and t[2] == "layout":
                section = t[0]
                continue
            if not section or len(t) != 5:
                continue
            symbol_map.append(DolphinSymbol(section, *t))
        return symbol_map


def ghidra_main():
    from ghidra.program.model.symbol import SourceType
    from ghidra.program.model.symbol import SymbolUtilities

    f = askFile("Load a Dolphin emulator symbol map", "Load")
    symbol_map = load_dolphin_map(f.getPath())

    for symbol in symbol_map:
        addr = toAddr(int(symbol.vaddr, 16))
        size = int(symbol.size, 16)
        name = SymbolUtilities.replaceInvalidChars(symbol.name, True)
        if symbol.section in [".init", ".text"]:
            createFunction(addr, symbol.name);
            success = getFunctionAt(addr) is not None
            if not success:
                print("Can't apply properties for symbol:"
                      " {0.vaddr} - {0.name}\n".format(symbol))
                createLabel(addr, name, True)
            else:
                getFunctionAt(addr).setName(name, SourceType.USER_DEFINED)
        else:
            createLabel(addr, name, True)


if __name__ == "__main__":
    ghidra_main()
