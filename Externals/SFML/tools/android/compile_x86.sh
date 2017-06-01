#!/usr/bin/env bash

DESTDIR=$PWD/tmp/x86

PATH=$PWD/toolchains/x86/bin:$PATH
CC=i686-linux-android-gcc
CXX=i686-linux-android-g++
CFLAGS=-I$DESTDIR/usr/include
CPPFLAGS=-I$DESTDIR/usr/include
LDFLAGS=-L$DESTDIR/usr/lib

./compile_libs.sh x86 $PATH $CC $CXX $CFLAGS $CPPFLAGS $LDFLAGS
