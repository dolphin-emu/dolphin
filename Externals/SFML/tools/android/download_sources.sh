#!/usr/bin/env bash

FLAC_VERSION=1.2.1
VORBIS_VERSION=1.3.3
OGG_VERSION=1.3.1

FLAC=flac-$FLAC_VERSION
VORBIS=libvorbis-$VORBIS_VERSION
OGG=libogg-$OGG_VERSION

SNDFILE_VERSION=1.0.25
SNDFILE=libsndfile-$SNDFILE_VERSION

JPEG_VERSION=9
JPEG=jpeg-$JPEG_VERSION

FREETYPE_VERSION=2.4.0
FREETYPE=freetype-$FREETYPE_VERSION

mkdir build

wget -nc -P src http://downloads.xiph.org/releases/flac/$FLAC.tar.gz
if [ ! -d "$PWD/tmp/$FLAC" ]
then
    tar -C build -xf src/$FLAC.tar.gz
fi

wget -nc -P src http://downloads.xiph.org/releases/vorbis/$VORBIS.tar.gz
if [ ! -d "$PWD/tmp/$VORBIS" ]
then
    tar -C build -xf src/$VORBIS.tar.gz
fi

wget -nc -P src http://downloads.xiph.org/releases/ogg/$OGG.tar.gz
if [ ! -d "$PWD/tmp/$OGG" ]
then
    tar -C build -xf src/$OGG.tar.gz
fi

wget -nc -P src http://www.mega-nerd.com/libsndfile/files/$SNDFILE.tar.gz
if [ ! -d "$PWD/tmp/$SNDFILE" ]
then
    tar -C build -xf src/$SNDFILE.tar.gz
fi

wget -nc -P src http://www.ijg.org/files/jpegsrc.v$JPEG_VERSION.tar.gz
if [ ! -d "$PWD/tmp/$JPEG" ]
then
    tar -C build -xf src/jpegsrc.v$JPEG_VERSION.tar.gz
fi

wget -nc -P src http://download.savannah.gnu.org/releases/freetype/$FREETYPE.tar.gz
if [ ! -d "$PWD/tmp/$FREETYPE" ]
then
    tar -C build -xf src/$FREETYPE.tar.gz
fi

wget -nc -P src https://github.com/AerialX/openal-soft-android/archive/master.tar.gz
if [ ! -d "$PWD/tmp/openal-soft-android-master" ]
then
    tar -C build -xf src/master.tar.gz
fi

patch build/openal-soft-android-master/CMakeLists.txt patches/remove-so-version-suffix.diff
