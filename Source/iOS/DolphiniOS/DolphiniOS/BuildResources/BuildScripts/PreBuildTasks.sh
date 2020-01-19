#!/bin/bash

set -e

if [ ! -x "$(command -v cmake)" ]; then
  echo "CMake is required to build Dolphin. Install CMake." >&2
  exit -1
fi

if [ ! -x "$(command -v ninja)" ]; then
  echo "Ninja is required to build Dolphin. Install ninja." >&2
  exit -1
fi

# Increment the build number
cd $PROJECT_DIR

INFO_FILE=$PROJECT_DIR/DolphiniOS/Info.plist
BUILD_NUMBER=$(/usr/libexec/PlistBuddy -c "Print CFBundleVersion" "$INFO_FILE")
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $(($BUILD_NUMBER + 1))" "$INFO_FILE"
