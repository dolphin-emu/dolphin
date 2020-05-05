#!/bin/bash

set -e

ROOT_SRC_DIR=$PROJECT_DIR/../
EXPORT_UUID=`uuidgen`
EXPORT_PATH="/tmp/DolphiniOS-NJB-$EXPORT_UUID"
DOLPHIN_EXPORT_PATH="$EXPORT_PATH/dolphin_ipa_root/"
APPLICATION_DESTINATION_PATH=$DOLPHIN_EXPORT_PATH/Payload/DolphiniOS.app
ENTITLEMENTS_FILE="$ROOT_SRC_DIR/DolphiniOS/DolphiniOS/BuildResources/Entitlements_PsychicPaper_Release.plist"

if [ $BUILD_FOR_PATREON == "YES" ]; then
  APPLICATION_DESTINATION_PATH=$DOLPHIN_EXPORT_PATH/Payload/DiOSPatreon.app
fi

# As recommended by saurik, don't copy dot files
export COPYFILE_DISABLE
export COPY_EXTENDED_ATTRIBUTES_DISABLE

# Make directories
mkdir -p $DOLPHIN_EXPORT_PATH
mkdir $DOLPHIN_EXPORT_PATH/Payload

exec > $EXPORT_PATH/archive.log 2>&1

# Copy resources
cp -r "$ARCHIVE_PATH/Products/Applications/DolphiniOS.app" $APPLICATION_DESTINATION_PATH

# Hack bundle ID
plutil -replace "CFBundleIdentifier" -string "me.oatmealdome.dolphinios-njb" $APPLICATION_DESTINATION_PATH/Info.plist

# Resign the application
codesign -f -s "OatmealDome Software" --entitlements $ENTITLEMENTS_FILE $APPLICATION_DESTINATION_PATH
codesign -f -s "OatmealDome Software" --entitlements $ENTITLEMENTS_FILE $APPLICATION_DESTINATION_PATH/Frameworks/*

# Remove the mobileprovision file
rm $APPLICATION_DESTINATION_PATH/embedded.mobileprovision

cd $DOLPHIN_EXPORT_PATH
zip -r ../DolphiniOS-NJB.ipa .

open $EXPORT_PATH
