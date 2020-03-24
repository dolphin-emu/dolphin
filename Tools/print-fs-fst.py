import argparse
import struct

def read_entry(f) -> dict:
    name = struct.unpack_from("12s", f.read(12))[0]
    uid = struct.unpack_from(">I", f.read(4))[0]
    gid = struct.unpack_from(">H", f.read(2))[0]
    is_file = struct.unpack_from("?", f.read(1))[0]
    modes = struct.unpack_from("BBB", f.read(3))
    attr = struct.unpack_from("B", f.read(2))[0]
    x3 = struct.unpack_from(">I", f.read(4))[0]
    num_children = struct.unpack_from(">I", f.read(4))[0]

    children = []
    for i in range(num_children):
        children.append(read_entry(f))

    return {
        "name": name,
        "uid": uid,
        "gid": gid,
        "is_file": is_file,
        "modes": modes,
        "attr": attr,
        "x3": x3,
        "children": children,
    }

COLOR_RESET = "\x1b[0;00m"
BOLD = "\x1b[0;37m"
COLOR_BLUE = "\x1b[1;34m"
COLOR_GREEN = "\x1b[0;32m"

def print_entry(entry, indent) -> None:
    mode_str = {0: "--", 1: "r-", 2: "-w", 3: "rw"}

    sp = ' ' * indent
    color = BOLD if entry["is_file"] else COLOR_BLUE

    owner = f"{COLOR_GREEN}{entry['uid']:04x}{COLOR_RESET}:{entry['gid']:04x}"
    attrs = f"{''.join(mode_str[mode] for mode in entry['modes'])}"
    other_attrs = f"{entry['attr']} {entry['x3']}"

    print(f"{sp}{color}{entry['name'].decode()}{COLOR_RESET} [{owner} {attrs} {other_attrs}]")
    for child in entry["children"]:
        print_entry(child, indent + 2)

def main() -> None:
    parser = argparse.ArgumentParser(description="Prints a FST in a tree-like format.")
    parser.add_argument("file")
    args = parser.parse_args()

    with open(args.file, "rb") as f:
        root = read_entry(f)

    print_entry(root, 0)

main()
