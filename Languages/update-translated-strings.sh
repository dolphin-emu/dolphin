#!/bin/bash

# This script goes through the translated .po files to ensure that they
# match dolphin-emu.pot and are correctly formatted. You must run this
# script before committing changes to the .po files, otherwise the .po
# files might get modified the next time you try to build using cmake.

cd "$(dirname "$0")/.."
POTFILE=./Languages/po/dolphin-emu.pot
find ./Languages/po -name '*.po' -exec msgmerge --quiet --update -N --backup=none -s {} $POTFILE \;

# This script takes all of the .po files and converts them into
# correctly formatted strings.xml files for Android.
python3 ./Languages/update_android_translated_strings.py
