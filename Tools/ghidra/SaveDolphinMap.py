# Copyright 2021 Dolphin Emulator Project
# Licensed under GPLv2+
# Refer to the license.txt file included.

#@category Dolphin

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


def ghidra_main():
    f = askFile("Save a Dolphin emulator symbol map", "Save")

    text_map = []
    for function in currentProgram.getListing().getFunctions(True):
        ea = int(function.getEntryPoint().toString(), 16)
        size = function.getBody().getNumAddresses()
        name = function.getName() + "({})".format(
            ", ".join(
                "{} {}".format(p.getDataType(), p.getName())
                for p in function.getParameters()
            )
        )
        text_map.append(
            DolphinSymbol(".text", ea, size, ea, 0, name)
        )

    data_map = []
    for data in currentProgram.getListing().getDefinedData(True):
        try:
            ea = int(data.getAddress().toString(), 16)
            size = data.getLength()
            name = data.getPathName()
            if name.startswith("DAT_") and \
                    data.getDataType().getName() not in ["string", "unicode"]:
                continue
            data_map.append(
                DolphinSymbol(".data", ea, size, ea, 0, name)
            )
        except:
            pass
    save_dolphin_map(f.getPath(), text_map, data_map)


if __name__ == "__main__":
    ghidra_main()
