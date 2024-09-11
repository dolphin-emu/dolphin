#!/usr/bin/env bash

DATE=$(git log -1 --pretty=%cd --date=iso8601 --date=format:'%Y-%m-%d')
sed -i -e "s/@DATE_PLACEHOLDER/${DATE}/" org.DolphinEmu.dolphin-emu.metainfo.xml
VERSION=$(git describe --tags)
sed -i -e "s/@VERSION_PLACEHOLDER/${VERSION}/" org.DolphinEmu.dolphin-emu.metainfo.xml


