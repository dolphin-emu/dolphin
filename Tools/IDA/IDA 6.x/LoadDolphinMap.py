# Copyright 2018 Dolphin Emulator Project
# Licensed under GPLv2+
# Refer to the LICENSES/GPL-2.0-or-later.txt file included.

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


def ida_main():
    import idc

    filepath = idc.AskFile(0, "*.map", "Load a Dolphin emulator symbol map")
    if filepath is None:
        return
    symbol_map = load_dolphin_map(filepath)

    for symbol in symbol_map:
        addr = int(symbol.vaddr, 16)
        size = int(symbol.size, 16)
        idc.MakeUnknown(addr, size, 0)
        if symbol.section in [".init", ".text"]:
            idc.MakeCode(addr)
            success = idc.MakeFunction(
                addr,
                idc.BADADDR if not size else (addr+size)
            )
        else:
            success = idc.MakeData(addr, idc.FF_BYTE, size, 0)

        if not success:
            idc.Message("Can't apply properties for symbol:"
                        " {0.vaddr} - {0.name}\n".format(symbol))

        flags = idc.SN_NOCHECK | idc.SN_PUBLIC
        if symbol.name.startswith("zz_"):
            flags |= idc.SN_AUTO | idc.SN_WEAK
        else:
            flags |= idc.SN_NON_AUTO
        idc.MakeNameEx(addr, symbol.name, flags)


if __name__ == "__main__":
    ida_main()
