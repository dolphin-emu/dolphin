#!/bin/bash

set -e

XCENT_FILE=$DERIVED_SOURCES_DIR/../DolphiniOS.app.xcent

# dynamic-codesigning is needed
if [ $BUILD_FOR_JAILBROKEN_DEVICE == "YES" ]
then
  plutil -replace "dynamic-codesigning" -bool YES $XCENT_FILE
else
  plutil -remove "dynamic-codesigning" $XCENT_FILE || true
fi
