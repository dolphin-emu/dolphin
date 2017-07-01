#!/bin/bash

# This script goes through the translated .po files to ensure that they
# match dolphin-emu.pot and are correctly formatted. You must run this
# script before committing changes to the .po files, otherwise the .po
# files might get modified the next time you try to build using cmake.

POTFILE=./Languages/po/dolphin-emu.pot
PO_FILES=$(find ./Languages/po -name '*.po')
for PO in $PO_FILES
do
	msgmerge --quiet --update --backup=none -s $PO $POTFILE
done
