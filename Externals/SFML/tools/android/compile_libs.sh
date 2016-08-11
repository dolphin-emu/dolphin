#!/usr/bin/env bash

DESTDIR=$PWD/tmp
LOCALDIR=$PWD

OPATH=$PATH

export PATH=$2
export CC=$3
export CXX=$4
export CFLAGS=$5
export CPPFLAGS=$6
export LDFLAGS=$7

if [ "$1" = "arm" ]
then
    ARCH=arm-linux
    ANDROID_ABI=armeabi

elif [ "$1" = "arm-v7a" ]
then
    ARCH=arm-linux
    ANDROID_ABI=armeabi-v7a

elif [ "$1" = "x86" ]
then
    ARCH=i686-linux
    ANDROID_ABI=x86

elif [ "$1" = "mips" ]
then
    ARCH=mips-linux
    ANDROID_ABI=mips
fi

HOST="--host=$ARCH"
PREFIX="--prefix=$DESTDIR/$1/usr need_version=no"

# Compile OGG
cd $LOCALDIR/build/libogg-* && sed -i 's/-version-info/-avoid-version/g' src/Makefile.in src/Makefile.am && ./configure $HOST $PREFIX --enable-shared=no && make && make install
rm $DESTDIR/$1/usr/lib/libogg*.so*

# Compile FLAC
cd  $LOCALDIR/build/flac-* && sed -i 's/-version-info/-avoid-version/g' src/libFLAC/Makefile.in src/libFLAC/Makefile.am && ./configure $HOST $PREFIX --enable-shared=no && make && make install
rm $DESTDIR/$1/usr/lib/libFLAC*.so*

# Compile VORBIS
cd  $LOCALDIR/build/libvorbis-* && sed -i 's/-version-info/-avoid-version/g' lib/Makefile.in lib/Makefile.am && ./configure $HOST $PREFIX --enable-shared=no && make && make install
rm $DESTDIR/$1/usr/lib/libvorbis*.so*

# Compile libsndfile (important: --disable-sqlite)
cd  $LOCALDIR/build/libsndfile-* && sed -i 's/-version-info/-avoid-version/g' src/Makefile.in src/Makefile.am && ./configure $HOST $PREFIX --disable-sqlite && make && make install

# Compile JPEG
cd $LOCALDIR/build/jpeg-* && sed -i 's/-version-info/-avoid-version/g' Makefile.in Makefile.am && ./configure $HOST $PREFIX && make && make install

# Compile freetype
cd  $LOCALDIR/build/freetype-* && sed -i 's/-version-info/-avoid-version/g' builds/unix/unix-cc.in && ./configure $HOST $PREFIX && make && make install

# Compile OpenAL-Soft
cd  $LOCALDIR/build/openal-soft-android-master && cd build && cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_CMAKE_TOOLCHAIN -DANDROID_ABI=$ANDROID_ABI -DANDROID_NATIVE_API_LEVEL=android-9 -DANDROID_USE_STLPORT=1 .. && make openal && mv libopenal.so $DESTDIR/$1/usr/lib

export PATH=$OPATH
