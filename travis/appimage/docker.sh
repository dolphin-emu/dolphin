#!/bin/bash -ex

curl -sLO “http://launchpadlibrarian.net/325446110/libusb-1.0-0-dev_1.0.21-2_amd64.deb”
curl -sLO “http://mirrors.kernel.org/ubuntu/pool/main/libu/libusb-1.0/libusb-1.0-0-dev_1.0.20-1_amd64.deb”
dpkg -i *.deb

cd /dolphin

mkdir build
cd build
cmake .. -G Ninja -DLINUX_LOCAL_DEV=true
ninja
ln -s ../../Data/Sys Binaries/

cd /tmp
curl -sLO "https://raw.githubusercontent.com/qurious-pixel/dolphin/zap/travis/appimage/appimage.sh"
chmod a+x appimage.sh
./appimage.sh
