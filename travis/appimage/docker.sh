#!/bin/bash -ex

cd /dolphin

mkdir build
cd build
cmake /dolphin-emu -DLINUX_LOCAL_DEV=true
make
ln -s ../../Data/Sys Binaries/
