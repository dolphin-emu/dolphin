#!/usr/bin/env bash

# Abort if no known installation of NDK
if [ "$NDK" == "" ]
then
    echo "Where is the NDK location ?"
    echo -n "NDK: "; read NDK
    export NDK=$NDK
fi

export ANDROID_NDK=$NDK

# Abort if we don't know the Android CMake toolchain location
if [ "$ANDROID_CMAKE_TOOLCHAIN" == "" ]
then
    echo "Where is the Android CMake toolchain ?"
    echo -n "ANDROID_CMAKE_TOOLCHAIN: "; read ANDROID_CMAKE_TOOLCHAIN
    export ANDROID_CMAKE_TOOLCHAIN=$ANDROID_CMAKE_TOOLCHAIN
fi

./clean_all.sh

./create_toolchains.sh

./download_sources.sh
./compile_arm.sh

rm -r $PWD/build
./download_sources.sh
./compile_x86.sh

rm -r $PWD/build
./download_sources.sh
./compile_mips.sh

rm -r $PWD/build
./download_sources.sh
./compile_arm-v7a.sh
