#!/bin/sh
# produce the MinGW binary files for snapshots
# !!!THIS SCRIPT IS FOR INTERNAL DEVELOPER USE ONLY!!!

PWD=`pwd`
cd ..
date=`date +%Y.%m.%d`
target=e:/dailies/$date
mkdir -p $target/include/libusb-1.0
cp -v libusb/libusb-1.0.def $target
cp -v libusb/libusb.h $target/include/libusb-1.0

#
# 32 bit binaries
#
target=e:/dailies/$date/MinGW32
git clean -fdx
# Not using debug (-g) in CFLAGS DRAMATICALLY reduces the size of the binaries
export CFLAGS="-O2 -m32"
export LDFLAGS="-m32"
export RCFLAGS="--target=pe-i386"
export DLLTOOLFLAGS="-m i386 -f --32"
echo `pwd`
(glibtoolize --version) < /dev/null > /dev/null 2>&1 && LIBTOOLIZE=glibtoolize || LIBTOOLIZE=libtoolize
$LIBTOOLIZE --copy --force || exit 1
aclocal || exit 1
autoheader || exit 1
autoconf || exit 1
automake -a -c || exit 1
./configure
make -j2
mkdir -p $target/static
mkdir -p $target/dll
cp -v libusb/.libs/libusb-1.0.a $target/static
cp -v libusb/.libs/libusb-1.0.dll $target/dll
cp -v libusb/.libs/libusb-1.0.dll.a $target/dll
make clean -j2

#
# 64 bit binaries
#
target=e:/dailies/$date/MinGW64
export CFLAGS="-O2"
export LDFLAGS=""
export RCFLAGS=""
export DLLTOOLFLAGS=""
./configure
make -j2
mkdir -p $target/static
mkdir -p $target/dll
cp -v libusb/.libs/libusb-1.0.a $target/static
cp -v libusb/.libs/libusb-1.0.dll $target/dll
cp -v libusb/.libs/libusb-1.0.dll.a $target/dll
cd $PWD