#!/bin/bash

# This script updates the dolphin-emu.pot file to match the strings in
# the source code.

cd "$(dirname "$0")/.."
SRCDIR=Source
find $SRCDIR -name '*.cpp' -o -name '*.h' -o -name '*.c' | \
	xgettext -d dolphin-emu -s --keyword=_ --keyword=wxTRANSLATE --keyword=SuccessAlertT \
	--keyword=PanicAlertT --keyword=PanicYesNoT --keyword=AskYesNoT --keyword=CriticalAlertT \
	--keyword=GetStringT --keyword=_trans --keyword=tr --keyword=QT_TR_NOOP \
	--add-comments=i18n -p ./Languages/po -o dolphin-emu.pot -f - --package-name="Dolphin Emulator"

sed -i "s/SOME DESCRIPTIVE TITLE\./Translation of dolphin-emu.pot to LANGUAGE/" Languages/po/dolphin-emu.pot
sed -i "s/YEAR THE PACKAGE'S COPYRIGHT HOLDER/2003-2013/" Languages/po/dolphin-emu.pot
sed -i "s/license as the PACKAGE package/license as the dolphin-emu package/" Languages/po/dolphin-emu.pot
