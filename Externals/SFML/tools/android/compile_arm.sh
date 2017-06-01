#!/usr/bin/env bash

DESTDIR=$PWD/tmp/arm

PATH=$PWD/toolchains/arm/bin:$PATH
CC=arm-linux-androideabi-gcc
CXX=arm-linux-androideabi-g++
CFLAGS=-I$DESTDIR/usr/include
CPPFLAGS=-I$DESTDIR/usr/include
LDFLAGS=-L$DESTDIR/usr/lib

./compile_libs.sh arm $PATH $CC $CXX $CFLAGS $CPPFLAGS $LDFLAGS
