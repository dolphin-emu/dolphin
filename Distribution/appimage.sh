!/bin/bash

# Environment variables
export ARCH=x86_64
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
wget -N https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -N https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
wget -N https://github.com/linuxdeploy/linuxdeploy-plugin-checkrt/releases/download/continuous/linuxdeploy-plugin-checkrt-x86_64.sh
wget -N https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage


chmod a+x linuxdeploy-x86_64.AppImage
chmod a+x linuxdeploy-plugin-qt-x86_64.AppImage
chmod a+x linuxdeploy-plugin-checkrt-x86_64.sh
chmod a+x appimagetool-x86_64.AppImage

# Build the AppImage
./linuxdeploy-x86_64.AppImage \
  --appdir AppDir \
  --plugin qt \
  --plugin checkrt

echo 'export QT_QPA_PLATFORM=xcb' >> ./AppDir/apprun-hooks/linuxdeploy-plugin-qt-hook.sh

./appimagetool-x86_64.AppImage ./AppDir root/