#!/bin/bash -ex

apt-get update -y
DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y libudev-dev libsfml-dev libminiupnpc-dev libmbedtls-dev libcurl4-openssl-dev libhidapi-dev libsystemd-dev libbluetooth-dev libpulse-dev libpugixml-dev libbz2-dev liblzo2-dev libpng-dev libusb-1.0-0-dev gettext \

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
