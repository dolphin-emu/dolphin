#!/bin/bash

# We define QT_XCB_NO_XI2 as a workaround for https://bugs.dolphin-emu.org/issues/12913.
QT_XCB_NO_XI2=1 $(dirname "$0")/dolphin-emu "$@"
