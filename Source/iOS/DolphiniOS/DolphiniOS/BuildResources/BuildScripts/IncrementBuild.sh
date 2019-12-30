#!/bin/bash

set -e

cd $PROJECT_DIR

INFO_FILE=$PROJECT_DIR/DolphiniOS/Info.plist
BUILD_NUMBER=$(/usr/libexec/PlistBuddy -c "Print CFBundleVersion" "$INFO_FILE")
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $(($BUILD_NUMBER + 1))" "$INFO_FILE"
