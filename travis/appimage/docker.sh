#!/bin/bash -ex

cd /dolphin

mkdir build
cd build
cmake .. -G Ninja -DLINUX_LOCAL_DEV=true
ninja
ln -s ../../Data/Sys Binaries/

cd /tmp
#curl -sLO "https://raw.githubusercontent.com/qurious-pixel/dolphin/zap/travis/appimage/appimage.sh"
#chmod a+x appimage.sh
./appimage.sh
