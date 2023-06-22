# Copyright 2021 Dolphin Emulator Project
# Licensed under GPLv2+
# Refer to the LICENSES/GPL-2.0-or-later.txt file included.

from collections import namedtuple


DolphinSymbol = namedtuple("DolphinSymbol", [
    "section", "addr", "size", "vaddr", "align", "name"
])


def save_dolphin_map(filepath, text_map, data_map):
    line = "{0.addr:08x} {0.size:08x} {0.vaddr:08x} {0.align} {0.name}\n"
    with open(filepath, "w") as f:
        f.write(".text section layout\n")
        for symbol in text_map:
            f.write(line.format(symbol))
        f.write("\n.data section layout\n")
        for symbol in data_map:
            f.write(line.format(symbol))


def ida_main():
    import idaapi
    import idautils
    import idc

    filepath = ida_kernwin.ask_file(1, "*.map", "Save a Dolphin emulator symbol map")
    if filepath is None:
        return
    text_map = []
    data_map = []
    for ea, name in idautils.Names():
        f = idaapi.get_func(ea)
        if f is not None:
            text_map.append(
                DolphinSymbol(".text", ea, f.size(), ea, 0, name)
            )
        else:
            data_map.append(
                DolphinSymbol(".data", ea, idc.get_item_size(ea), ea, 0, name)
            )
    save_dolphin_map(filepath, text_map, data_map)


if __name__ == "__main__":
    ida_main()
