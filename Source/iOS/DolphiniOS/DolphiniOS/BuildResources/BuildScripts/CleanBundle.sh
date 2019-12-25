#!/bin/bash

set -e

DOLPHIN_APP_PATH=$CODESIGNING_FOLDER_PATH

if [ -z "$DOLPHIN_APP_PATH" ]; then
  exit -1
fi

# We don't need some folders in Sys, since they are for the Qt version.
rm -rf $DOLPHIN_APP_PATH/Sys/Themes
rm -rf $DOLPHIN_APP_PATH/Sys/Resources
