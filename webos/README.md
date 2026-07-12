# Dolphin on webOS (experimental)

Initial port scaffolding modeled on RetroArch's `Makefile.webos` flow:
webosbrew native-toolchain → CMake (NoGUI) → `ares-package` IPK.

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
- **NoGUI + Wayland** windowing (`-p wayland`, default on webOS) using `wl_webos_shell`
- GLES via Dolphin's **EGL** (`GLContextEGLWayland`, aligned with [#14652](https://github.com/dolphin-emu/dolphin/pull/14652))
- Qt / Vulkan / X11 / SDL disabled
- Packaging ships `dolphin-emu-nogui` + `Sys` + runtime libs
- GCC 12 softfp: Release C++ uses `-O1` (avoids ICE in libstdc++ `<limits>` at `-O2`); parallel builds can still ICE — retry with `-j2` if needed

### aarch64

Makefile supports `aarch64-webos-linux-gnu-` (JIT-capable `_M_ARM_64`). Prebuilt SDKs are **Linux-hosted only** ([cscd98/buildroot-nc4](https://github.com/cscd98/buildroot-nc4/releases)):

```sh
# On Linux amd64 / or with Docker Desktop running on macOS:
./webos/build-aarch64-docker.sh
```

The Docker helper uses `linux/amd64` + `aarch64-webos-linux-gnu_sdk-buildroot-x86_64.tar.gz`, then applies RetroArch-style ELF interpreter / IPK arch patching for the 64→32 bridge.

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
