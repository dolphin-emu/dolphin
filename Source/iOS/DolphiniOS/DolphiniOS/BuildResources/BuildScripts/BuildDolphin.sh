#!/bin/bash

set -e

ROOT_DOLPHIN_DIR=$PROJECT_DIR/../../..
CMAKE_BUILD_DIR=$ROOT_DOLPHIN_DIR/build-$ARCHS-$DOLPHIN_BUILD_TYPE
GENERIC_BUILD_DIR=$ROOT_DOLPHIN_DIR/build-$DOLPHIN_BUILD_TYPE
ADDITIONAL_CMAKE_SETTINGS=

IOS_PLATFORM=OS64
if [ $ARCHS == "x86_64" ]; then
    IOS_PLATFORM=SIMULATOR64
fi

if [ $BUILD_FOR_JAILBROKEN_DEVICE == "YES" ]; then
  CMAKE_BUILD_DIR="$CMAKE_BUILD_DIR-jb"
  ADDITIONAL_CMAKE_SETTINGS="-DIOS_JAILBROKEN=1"
fi

if [ ! -d "$CMAKE_BUILD_DIR" ]; then
    mkdir $CMAKE_BUILD_DIR
    cd $CMAKE_BUILD_DIR
    
    cmake $ROOT_DOLPHIN_DIR -GNinja -DCMAKE_TOOLCHAIN_FILE=$ROOT_DOLPHIN_DIR/Source/iOS/ios.toolchain.cmake -DPLATFORM=$IOS_PLATFORM -DDEPLOYMENT_TARGET="12.0" -DCMAKE_BUILD_TYPE=$DOLPHIN_BUILD_TYPE -DENABLE_ANALYTICS=NO $ADDITIONAL_CMAKE_SETTINGS
fi

cd $CMAKE_BUILD_DIR

ninja

if [ ! -d "$GENERIC_BUILD_DIR" ]; then
    mkdir $GENERIC_BUILD_DIR
    mkdir $GENERIC_BUILD_DIR/libs
    mkdir $GENERIC_BUILD_DIR/libs/Dolphin
    mkdir $GENERIC_BUILD_DIR/libs/Externals
fi

rm -f $GENERIC_BUILD_DIR/libs/Dolphin/*.a
rm -f $GENERIC_BUILD_DIR/libs/Externals/*.a

find Source/ -name '*.a' -exec ln '{}' "$GENERIC_BUILD_DIR/libs/Dolphin/" ';'

find Externals/ -name '*.a' -exec ln '{}' "$GENERIC_BUILD_DIR/libs/Externals/" ';'

if [ -f "$GENERIC_BUILD_DIR/libs/Externals/libfmtd.a" ]; then
    rm $GENERIC_BUILD_DIR/libs/Externals/libfmt.a || true
    cp $GENERIC_BUILD_DIR/libs/Externals/libfmtd.a $GENERIC_BUILD_DIR/libs/Externals/libfmt.a
fi

rm $PROJECT_DIR/libMoltenVK.dylib || true
ln -s $ROOT_DOLPHIN_DIR/Externals/MoltenVK/libvulkan_iOS.dylib $PROJECT_DIR/libMoltenVK.dylib
