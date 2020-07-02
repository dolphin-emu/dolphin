#!/bin/bash

set -e

ROOT_SRC_DIR=$PROJECT_DIR/../
EXPORT_UUID=`uuidgen`
EXPORT_PATH="/tmp/DolphiniOS-NJB-$EXPORT_UUID"
DOLPHIN_EXPORT_PATH="$EXPORT_PATH/dolphin_ipa_root/"
APPLICATION_DESTINATION_PATH=$DOLPHIN_EXPORT_PATH/Payload/DolphiniOS.app
CODESIGN_ARGS='-f -s 0059C24602FCB60DA511DCAA08727C70C1FF9A99'
ENTITLEMENTS_FILE="$ROOT_SRC_DIR/DolphiniOS/DolphiniOS/BuildResources/Entitlements_NJB.plist"
BUNDLE_ID="me.oatmealdome.DolphiniOS-njb"
BUILD_NUMBER=$(/usr/libexec/PlistBuddy -c "Print CFBundleVersion" "$PROJECT_DIR/DolphiniOS/Info.plist")

if [ -n "$IS_CI" ]; then
  CODESIGN_ARGS+=" --keychain $1"
fi

if [ $BUILD_FOR_PATREON == "YES" ]; then
  BUNDLE_ID="me.oatmealdome.DolphiniOS-njb-patreon-beta"
  APPLICATION_DESTINATION_PATH=$DOLPHIN_EXPORT_PATH/Payload/DiOSPatreon.app
fi

# As recommended by saurik, don't copy dot files
export COPYFILE_DISABLE
export COPY_EXTENDED_ATTRIBUTES_DISABLE

# Make directories
mkdir -p $DOLPHIN_EXPORT_PATH
mkdir $DOLPHIN_EXPORT_PATH/Payload

if [ -z "$IS_CI" ]; then
  exec > $EXPORT_PATH/archive.log 2>&1
fi

# Copy resources
cp -r "$ARCHIVE_PATH/Products/Applications/DolphiniOS.app" $APPLICATION_DESTINATION_PATH

# Hack bundle ID
plutil -replace "CFBundleIdentifier" -string "$BUNDLE_ID" $APPLICATION_DESTINATION_PATH/Info.plist

# Resign the application
codesign $CODESIGN_ARGS --entitlements $ENTITLEMENTS_FILE $APPLICATION_DESTINATION_PATH
codesign $CODESIGN_ARGS --entitlements $ENTITLEMENTS_FILE $APPLICATION_DESTINATION_PATH/Frameworks/*

# Remove the mobileprovision file
rm $APPLICATION_DESTINATION_PATH/embedded.mobileprovision || true

cd $DOLPHIN_EXPORT_PATH
zip -r ../DolphiniOS-NJB.ipa .

if [ -n "$IS_CI" ]; then
  echo ::set-env name=$2::$EXPORT_PATH
else
  open $EXPORT_PATH
fi
