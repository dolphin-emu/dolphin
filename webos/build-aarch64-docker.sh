#!/usr/bin/env bash
# Cross-build Dolphin for webOS aarch64 inside Linux (Docker).
# Requires Docker Desktop running. Host OS can be macOS.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK_TGZ="${WEBOS_AARCH64_SDK_TGZ:-/Users/roman/work/webos-sdk/aarch64-webos-linux-gnu_sdk-buildroot-x86_64.tar.gz}"
IMAGE="${WEBOS_DOCKER_IMAGE:-ubuntu:22.04}"

if [[ ! -f "$SDK_TGZ" ]]; then
  echo "Missing aarch64 SDK tarball: $SDK_TGZ" >&2
  echo "Download from https://github.com/cscd98/buildroot-nc4/releases (webos-2ec78a0)" >&2
  exit 1
fi

if ! docker info >/dev/null 2>&1; then
  echo "Docker is not running. Start Docker Desktop and retry." >&2
  exit 1
fi

docker run --rm --platform linux/amd64 \
  -v "$ROOT:/src" \
  -v "$SDK_TGZ:/sdk.tar.gz:ro" \
  -w /src \
  "$IMAGE" \
  bash -lc '
    set -euo pipefail
    apt-get update -qq
    apt-get install -y -qq cmake ninja-build python3 git pkg-config wget ca-certificates xz-utils \
      patchelf binutils
    # Prefer host ares-package via volume; fall back to downloading ares-cli-rs if absent.
    if ! command -v ares-package >/dev/null 2>&1; then
      echo "ares-package not in image; packaging will be done on the host after the binary builds."
    fi
    mkdir -p /opt/webos-aarch64
    tar -xzf /sdk.tar.gz -C /opt/webos-aarch64 --strip-components=0
    SDK_DIR=$(find /opt/webos-aarch64 -maxdepth 2 -type d -name "aarch64-webos-linux-gnu_sdk-buildroot" | head -1)
    if [[ -z "${SDK_DIR}" ]]; then
      SDK_DIR=$(find /opt/webos-aarch64 -maxdepth 1 -mindepth 1 -type d | head -1)
    fi
    (cd "$SDK_DIR" && ./relocate-sdk.sh)
    source "$SDK_DIR/environment-setup"
    which "${CROSS_COMPILE}gcc"
    make -f Makefile.webos clean || true
    # Build binary inside container; package with host ares-package afterward if needed.
    make -f Makefile.webos -j"$(nproc)" build
    # Attempt IPK packaging; ignore missing ares-package so host can finish.
    make -f Makefile.webos ipk || {
      echo "IPK packaging failed in container (likely missing ares-package)."
      echo "Binary is ready at build-webos-aarch64/Binaries/dolphin-emu-nogui"
      echo "Run on host: (cd webos && ares-package dist && ./patch-aarch64-in-ipk.sh org.dolphinemu.webos_*_aarch64.ipk)"
      exit 0
    }
    ls -lh webos/*.ipk
  '
