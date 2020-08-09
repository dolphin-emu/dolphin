#!/bin/bash -ex

cd /dolphin

mkdir build
cd build
cmake .. -G Ninja -DLINUX_LOCAL_DEV=true
ninja
ln -s ../../Data/Sys Binaries/
