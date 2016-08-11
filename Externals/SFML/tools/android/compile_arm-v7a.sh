#!/usr/bin/env bash

DESTDIR=$PWD/tmp/arm-v7a

PATH=$PWD/toolchains/arm/bin:$PATH
CC=arm-linux-androideabi-gcc
CXX=arm-linux-androideabi-g++
CFLAGS="-I$DESTDIR/usr/include -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16"
CPPFLAGS="-I$DESTDIR/usr/include -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16"
LDFLAGS="-L$DESTDIR/usr/lib -march=armv7-a -Wl,--fix-cortex-a8"

./compile_libs.sh arm-v7a $PATH $CC $CXX "$CFLAGS" "$CPPFLAGS" "$LDFLAGS"
