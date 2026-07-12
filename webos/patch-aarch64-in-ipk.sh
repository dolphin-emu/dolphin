#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <package.ipk>"
  exit 1
fi

IPK="$1"
IPK="$(cd "$(dirname "$IPK")" && pwd)/$(basename "$IPK")"
IPK_DIR="$(dirname "$IPK")"
IPK_BASE="$(basename "$IPK")"
OUTNAME="${IPK_BASE%.ipk}-patched.ipk"

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

echo "[*] Working in: $WORKDIR"

cp "$IPK" "$WORKDIR/"
cd "$WORKDIR"

echo "[*] Extracting IPK..."
ar x "$IPK_BASE"

echo "[*] Extracting control..."
mkdir control
tar -xf control.tar.gz -C control

echo "[*] Patching Architecture field..."
# Portable in-place sed (GNU and BSD).
if sed --version >/dev/null 2>&1; then
  sed -i 's/^Architecture: *aarch64/Architecture: arm/' control/control
else
  sed -i '' 's/^Architecture: *aarch64/Architecture: arm/' control/control
fi

echo "[*] Rebuilding control..."
tar -czf control.tar.gz -C control control

echo "[*] Repacking..."
# Do not use `ar s` / ranlib — on macOS that corrupts non-Mach-O IPK members.
# Write a SysV ar archive directly (same layout ares-package / dpkg uses).
python3 - "$OUTNAME" debian-binary control.tar.gz data.tar.gz <<'PY'
import os, struct, sys

out = sys.argv[1]
members = sys.argv[2:]

def ar_header(name: str, size: int) -> bytes:
    # GNU/SysV ar header: 60 bytes
    name_field = (name + "/").ljust(16)[:16].encode("ascii")
    mtime = b"0".ljust(12)
    uid = b"0".ljust(6)
    gid = b"0".ljust(6)
    mode = b"100644".ljust(8)
    size_field = str(size).ljust(10).encode("ascii")
    magic = b"`\n"
    return name_field + mtime + uid + gid + mode + size_field + magic

with open(out, "wb") as f:
    f.write(b"!<arch>\n")
    for path in members:
        data = open(path, "rb").read()
        name = os.path.basename(path)
        f.write(ar_header(name, len(data)))
        f.write(data)
        if len(data) % 2 == 1:
            f.write(b"\n")
print(out)
PY

echo "[*] Copying result back..."
mv "$OUTNAME" "$IPK_DIR/"

echo "[+] Done: $IPK_DIR/$OUTNAME"
