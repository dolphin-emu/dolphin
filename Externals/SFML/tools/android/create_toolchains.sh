#!/usr/bin/env bash

create_toolchain () {

    # abort if already created
    if [ -d "$PWD/toolchains/$2" ]
    then
        return
    fi

    # save the working directory and move to the NDK directory
    WORKING_DIRECTORY=$PWD
    cd $NDK

    # prepare the command according to chosen options
    PLATFORM=--platform=android-$1
    DIR=--install-dir=$WORKING_DIRECTORY/toolchains/$2
    MAKE=$NDK/build/tools/make-standalone-toolchain.sh

    if [ "$2" = "arm" ]
    then
        TOOLCHAIN=--toolchain=arm-linux-androideabi-4.8
    elif [ "$2" = "x86" ]
    then
        TOOLCHAIN=--toolchain=x86-4.8
    elif [ "$2" = "mips" ]
    then
        TOOLCHAIN=--toolchain=mipsel-linux-android-4.8
    else
        echo "Abort."
        exit 1
    fi

    # create the standalone toolchain
    $MAKE $PLATFORM $TOOLCHAIN $DIR --stl=libc++

    # go back to our working directory
    cd $WORKING_DIRECTORY

    # move linux/soundcard.h to sys/soundcard.h
    mv $PWD/toolchains/$2/sysroot/usr/include/linux/soundcard.h $PWD/toolchains/$2/sysroot/usr/include/sys
}

create_toolchain 9 arm
create_toolchain 9 x86
create_toolchain 9 mips
