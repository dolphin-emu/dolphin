#!/bin/bash

# This script signs a specific object with the specified identity, entitlements,
# and optional flags. If the target is a bundle, it will also sign all frameworks
# and dylibs within the bundle.

set -eu

function usage() {
  echo "Usage: $0 [-t] [-e <entitlements file or "preserve">] <identity> <target to codesign>"
  exit 1
}

USE_SECURE_TIMESTAMP=0
ENTITLEMENTS_FILE=""

while getopts ":te:" opt; do
  case $opt in
    t)
      USE_SECURE_TIMESTAMP=1
      ;;
    e)
      ENTITLEMENTS_FILE=$OPTARG
      ;;
    \?)
      usage
      ;;
  esac
done

if [ $USE_SECURE_TIMESTAMP -eq 1 ]; then
  TIMESTAMP_FLAG="--timestamp"
else
  TIMESTAMP_FLAG="--timestamp=none"
fi

shift $((OPTIND - 1))

if [ $# -ne 2 ]; then
  usage
fi

IDENTITY=$1
TARGET_PATH=$2

# Signs the given target with the specified identity and optional flags.
function sign() {
  /usr/bin/codesign -f -s "$IDENTITY" $TIMESTAMP_FLAG ${2:-} "$1"
}

if [ -d "$TARGET_PATH" ]; then
  # Newlines are the only valid separator character in find's output.
  IFS=$'\n'

  for framework in $(find "$TARGET_PATH" -depth -not -path "*/Helpers/*" -name '*.dylib' -or -name '*.framework'); do
    sign "$framework"
  done

  unset IFS
fi

TARGET_EXTRA_CODESIGN_FLAGS="-o runtime"

if [ -n "$ENTITLEMENTS_FILE" ]; then
  # "preserve" is a special keyword which tells us we should preserve the
  # existing entitlements in the target.
  if [ "$ENTITLEMENTS_FILE" == "preserve" ]; then
    TARGET_EXTRA_CODESIGN_FLAGS+=" --preserve-metadata=entitlements"
  else
    TARGET_EXTRA_CODESIGN_FLAGS+=" --entitlements $ENTITLEMENTS_FILE"
  fi
fi

sign "$TARGET_PATH" "$TARGET_EXTRA_CODESIGN_FLAGS"
