#!/bin/bash

set -e

ROOT_SRC_DIR=$PROJECT_DIR/../
EXPORT_UUID=`uuidgen`
EXPORT_PATH="/tmp/DolphiniOS-$EXPORT_UUID"
DOLPHIN_EXPORT_PATH="$EXPORT_PATH/dolphin_deb_root/"
CSDBGD_EXPORT_PATH="$EXPORT_PATH/csdbgd_deb_root/"
CONTROL_FILE=$ROOT_SRC_DIR/DolphiniOS/DolphiniOS/BuildResources/DebFiles/control.in
APPLICATION_DESTINATION_PATH=$DOLPHIN_EXPORT_PATH/Applications/DolphiniOS.app
CODESIGN_ARGS='-f -s 0059C24602FCB60DA511DCAA08727C70C1FF9A99'
BUNDLE_ID="me.oatmealdome.DolphiniOS"
VERSION_STRING=$(cat $PROJECT_DIR/DolphiniOS.xcodeproj/project.pbxproj | grep -m1 'MARKETING_VERSION' | cut -d'=' -f2 | tr -d ';' | tr -d ' ')
BUILD_NUMBER=$(/usr/libexec/PlistBuddy -c "Print CFBundleVersion" "$PROJECT_DIR/DolphiniOS/Info.plist")

if [ -n "$IS_CI" ]; then
  CODESIGN_ARGS+=" --keychain $1"
fi

if [ $BUILD_FOR_PATREON == "YES" ]; then
  CONTROL_FILE=$ROOT_SRC_DIR/DolphiniOS/DolphiniOS/BuildResources/DebFiles/control-patreon.in
  APPLICATION_DESTINATION_PATH=$DOLPHIN_EXPORT_PATH/Applications/DiOSPatreon.app
  BUNDLE_ID="me.oatmealdome.DolphiniOS-patreon-beta"
fi

# As recommended by saurik, don't copy dot files
export COPYFILE_DISABLE
export COPY_EXTENDED_ATTRIBUTES_DISABLE

# Make directories
mkdir -p $DOLPHIN_EXPORT_PATH
mkdir $DOLPHIN_EXPORT_PATH/Applications
mkdir $DOLPHIN_EXPORT_PATH/DEBIAN

mkdir -p $CSDBGD_EXPORT_PATH
mkdir -p $CSDBGD_EXPORT_PATH/Library/LaunchDaemons
mkdir -p $CSDBGD_EXPORT_PATH/usr/libexec
mkdir $CSDBGD_EXPORT_PATH/DEBIAN

if [ -z "$IS_CI" ]; then
  exec > $EXPORT_PATH/archive.log 2>&1
fi

# Copy resources
cp -r "$ARCHIVE_PATH/Products/Applications/DolphiniOS.app" $APPLICATION_DESTINATION_PATH
cp $ROOT_SRC_DIR/DolphiniOS/DolphiniOS/BuildResources/DebFiles/postinst.sh $DOLPHIN_EXPORT_PATH/DEBIAN/postinst
cp $ROOT_SRC_DIR/DolphiniOS/DolphiniOS/BuildResources/DebFiles/postrm.sh $DOLPHIN_EXPORT_PATH/DEBIAN/postrm
chmod -R 755 $DOLPHIN_EXPORT_PATH/DEBIAN/*
sed "s/VERSION_NUMBER/$VERSION_STRING-$BUILD_NUMBER/g" $CONTROL_FILE > $DOLPHIN_EXPORT_PATH/DEBIAN/control

cp $ROOT_SRC_DIR/csdbgd/csdbgd $CSDBGD_EXPORT_PATH/usr/libexec
cp $ROOT_SRC_DIR/csdbgd/me.oatmealdome.csdbgd.plist $CSDBGD_EXPORT_PATH/Library/LaunchDaemons
cp $ROOT_SRC_DIR/csdbgd/DebFiles/preinst.sh $CSDBGD_EXPORT_PATH/DEBIAN/preinst
cp $ROOT_SRC_DIR/csdbgd/DebFiles/postinst.sh $CSDBGD_EXPORT_PATH/DEBIAN/postinst
cp $ROOT_SRC_DIR/csdbgd/DebFiles/prerm.sh $CSDBGD_EXPORT_PATH/DEBIAN/prerm
chmod -R 755 $CSDBGD_EXPORT_PATH/DEBIAN/*
sed "s/VERSION_NUMBER/$VERSION_STRING-$BUILD_NUMBER/g" $ROOT_SRC_DIR/csdbgd/DebFiles/control > $CSDBGD_EXPORT_PATH/DEBIAN/control

# Hack bundle ID
plutil -replace "CFBundleIdentifier" -string "$BUNDLE_ID" $APPLICATION_DESTINATION_PATH/Info.plist

# Resign the application
codesign $CODESIGN_ARGS --entitlements $ROOT_SRC_DIR/DolphiniOS/DolphiniOS/BuildResources/Entitlements_JB.plist $APPLICATION_DESTINATION_PATH
codesign $CODESIGN_ARGS --entitlements $ROOT_SRC_DIR/DolphiniOS/DolphiniOS/BuildResources/Entitlements_JB.plist $APPLICATION_DESTINATION_PATH/Frameworks/*
codesign $CODESIGN_ARGS --entitlements $ROOT_SRC_DIR/csdbgd/Entitlements.plist $CSDBGD_EXPORT_PATH/usr/libexec/csdbgd

# Remove the mobileprovision file
rm $APPLICATION_DESTINATION_PATH/embedded.mobileprovision || true

# Create the deb file
dpkg-deb -b $DOLPHIN_EXPORT_PATH
mv $EXPORT_PATH/dolphin_deb_root.deb $EXPORT_PATH/DolphiniOS.deb

dpkg-deb -b $CSDBGD_EXPORT_PATH
mv $EXPORT_PATH/csdbgd_deb_root.deb $EXPORT_PATH/csdbgd.deb

if [ -n "$IS_CI" ]; then
  echo ::set-env name=$2::$EXPORT_PATH
else
  open $EXPORT_PATH
fi
