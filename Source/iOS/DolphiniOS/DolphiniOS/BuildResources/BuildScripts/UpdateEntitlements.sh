#!/bin/bash

set -e

XCENT_FILE=$DERIVED_SOURCES_DIR/../DolphiniOS.app.xcent

if [ ! -f "$XCENT_FILE" ]
then
  exit 0
fi

# dynamic-codesigning is needed
if [ $BUILD_FOR_JAILBROKEN_DEVICE == "YES" ]
then
  /usr/libexec/PlistBuddy -x -c "Merge $PROJECT_DIR/DolphiniOS/BuildResources/Entitlements_JB.plist" "$XCENT_FILE"
fi
