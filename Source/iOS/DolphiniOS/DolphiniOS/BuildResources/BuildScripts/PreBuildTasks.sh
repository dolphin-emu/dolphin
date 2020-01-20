#!/bin/bash

set -e

CheckCommand()
{
  if [ ! -x "$(command -v $1)" ]; then
    echo "$1 is required to build Dolphin. Install $1 with brew." >&2
    exit -1
  fi
}

CheckCommand "cmake"

CheckCommand "ninja"

CheckCommand "/usr/local/bin/python3"

ROOT_DOLPHIN_DIR=$PROJECT_DIR/../../..

cd $PROJECT_DIR

# Update the strings
/usr/local/bin/python3 $PROJECT_DIR/DolphiniOS/BuildResources/Tools/UpdateDolphinStrings.py $ROOT_DOLPHIN_DIR/Languages/po $PROJECT_DIR/DolphiniOS/Localizables/

/usr/local/bin/python3 $PROJECT_DIR/DolphiniOS/BuildResources/Tools/UpdateUIStrings.py $ROOT_DOLPHIN_DIR/Languages/po $PROJECT_DIR/DolphiniOS/UI/

# Increment the build number
INFO_FILE=$PROJECT_DIR/DolphiniOS/Info.plist
BUILD_NUMBER=$(/usr/libexec/PlistBuddy -c "Print CFBundleVersion" "$INFO_FILE")
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $(($BUILD_NUMBER + 1))" "$INFO_FILE"
