#!/bin/bash

# Environment variables
if [ "$(uname -m)" = "x86_64" ];
then
    export ARCH=x86_64
    echo "CPU architecture detected as x86_64."

elif [ "$(uname -m)" = "aarch64" ];
then
    export ARCH=aarch64
    echo "CPU architecture detected as aarch64."

else
    echo "CPU architecture not supported or detected."
    exit 1
fi

export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=/usr/lib/qt6/bin/qmake

# Prepare the AppDir
DESTDIR=./AppDir ninja install
mkdir -p ./AppDir/usr/Source/Core
cp -r ./Source/Core/DolphinQt ./AppDir/usr/Source/Core
rm -rf ./AppDir/usr/Source/Core/DolphinQt/CMakeFiles
rm -rf ./AppDir/usr/Source/Core/DolphinQt/dolphin-emu_autogen
rm ./AppDir/usr/Source/Core/DolphinQt/cmake_install.cmake
rm ./AppDir/usr/bin/dolphin-emu-nogui
rm ./AppDir/usr/bin/dolphin-tool
mv ./AppDir/usr/share/dolphin-emu/sys ./AppDir/usr/bin/Sys
rm -rf ./AppDir/usr/share/dolphin-emu
sed -i 's/env QT_QPA_PLATFORM=xcb dolphin-emu/dolphin-emu/g' ./AppDir/usr/share/applications/dolphin-emu.desktop

# Prepare Tools for building the AppImage
wget -N https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage
wget -N https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage
wget -N https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH}.AppImage


chmod a+x linuxdeploy-${ARCH}.AppImage
chmod a+x linuxdeploy-plugin-qt-${ARCH}.AppImage
chmod a+x appimagetool-${ARCH}.AppImage

# Build the AppImage
./linuxdeploy-${ARCH}.AppImage \
  --appdir AppDir \
  --plugin qt

echo 'env QT_QPA_PLATFORM=xcb' >> ./AppDir/apprun-hooks/linuxdeploy-plugin-qt-hook.sh

./appimagetool-${ARCH}.AppImage ./AppDir
