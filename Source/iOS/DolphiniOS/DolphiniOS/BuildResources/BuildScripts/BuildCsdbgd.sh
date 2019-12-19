#!/bin/bash

set -e

ROOT_DOLPHIN_DIR=$PROJECT_DIR/../../..

if [ $BUILD_FOR_JAILBROKEN_DEVICE == "YES" ]; then
  cd $ROOT_DOLPHIN_DIR/Source/iOS/csdbgd
  make
fi
