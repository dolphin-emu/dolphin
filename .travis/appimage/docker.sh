#!/bin/bash -ex

cd $HOME

mkdir build
cd build
cmake /dolphin -DLINUX_LOCAL_DEV=true
make
ln -s ../../Data/Sys Binaries/
