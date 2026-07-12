# Dolphin on webOS (experimental)

Initial port scaffolding modeled on RetroArch's `Makefile.webos` flow:
webosbrew native-toolchain → CMake (NoGUI / headless) → `ares-package` IPK.

## Prerequisites

1. [webosbrew/native-toolchain](https://github.com/webosbrew/native-toolchain) for your host
2. Run `relocate-sdk.sh` inside the extracted SDK
3. On macOS, if `arm-webos-linux-gnueabi-gcc` fails with a `readlink` error from `toolchain-wrapper`, replace that wrapper with a shell script that invokes `*.br_real --sysroot=...` (see notes below)
4. `ares-package` (`ares-cli` / webOS Dev Manager CLI)
5. Dolphin submodules: `git submodule update --init --recursive`

## Build

```sh
source /path/to/arm-webos-linux-gnueabi_sdk-buildroot/environment-setup
make -f Makefile.webos clean
make -f Makefile.webos -j$(getconf _NPROCESSORS_ONLN) ipk
```

Output: `webos/org.dolphinemu.webos_<version>_arm.ipk`

## Install / launch

```sh
make -f Makefile.webos launch
```

## Current scope

- **armhf** (32-bit) JIT-less build via `ENABLE_GENERIC`
- **NoGUI** frontend, **headless** platform (no on-TV window yet)
- Qt / Vulkan / X11 / tests disabled
- Packaging ships `dolphin-emu-nogui` + `Sys` + a few runtime libs
- GCC 12 softfp: Release C++ uses `-O1` (avoids an ICE in libstdc++ `<limits>` at `-O2`)

Verified locally: `make -f Makefile.webos ipk` produces `webos/org.dolphinemu.webos_0.0.1_arm.ipk`.

Next steps toward a usable TV app: SDL2 or Wayland/EGL window platform, aarch64 + 64→32 bridge (like RetroArch), input, and performance work.

## macOS toolchain-wrapper workaround

If compilers error with:

```text
toolchain-wrapper.c: readlink: No such file or directory
```

in `$SDK/bin`, replace `toolchain-wrapper` with:

```sh
#!/bin/bash
set -euo pipefail
WRAPPER_DIR="$(cd "$(dirname "$0")" && pwd)"
PROG="$(basename "$0")"
REAL="${WRAPPER_DIR}/${PROG}.br_real"
SYSROOT="$(cd "${WRAPPER_DIR}/../arm-webos-linux-gnueabi/sysroot" && pwd)"
exec "$REAL" --sysroot="$SYSROOT" "$@"
```

and keep the usual `arm-webos-linux-gnueabi-{gcc,g++,c++,cpp}` symlinks pointing at it.
