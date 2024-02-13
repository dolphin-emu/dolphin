#!/bin/bash -e
# build-appimage.sh

NETPLAY_APPIMAGE_STRING="Slippi_Netplay_Mainline-x86_64.AppImage"
PLAYBACK_APPIMAGE_STRING="Slippi_Playback_Mainline-x86_64.AppImage"

LINUXDEPLOY_PATH="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous"
LINUXDEPLOY_FILE="linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_URL="${LINUXDEPLOY_PATH}/${LINUXDEPLOY_FILE}"

LINUXDEPLOY_QT_PLUGIN_PATH="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20240109-1"
LINUXDEPLOY_QT_PLUGIN_FILE="linuxdeploy-plugin-qt-x86_64.AppImage"
LINUXDEPLOY_QT_PLUGIN_URL="${LINUXDEPLOY_QT_PLUGIN_PATH}/${LINUXDEPLOY_QT_PLUGIN_FILE}"

UPDATEPLUG_PATH="https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous"
UPDATEPLUG_FILE="linuxdeploy-plugin-appimage-x86_64.AppImage"
UPDATEPLUG_URL="${UPDATEPLUG_PATH}/${UPDATEPLUG_FILE}"

UPDATETOOL_PATH="https://github.com/AppImage/AppImageUpdate/releases/download/continuous"
UPDATETOOL_FILE="appimageupdatetool-x86_64.AppImage"
UPDATETOOL_URL="${UPDATETOOL_PATH}/${UPDATETOOL_FILE}"

PLAYBACK_CODES_PATH="./Data/PlaybackGeckoCodes/"

APPDIR_BIN="./AppDir/usr/bin"
APPDIR_HOOKS="./AppDir/apprun-hooks"

export NO_STRIP=on

# Grab various appimage binaries from GitHub if we don't have them
if [ ! -e ./Tools/linuxdeploy ]; then
	wget ${LINUXDEPLOY_URL} -O ./Tools/linuxdeploy
fi
if [ ! -e "./Tools/${LINUXDEPLOY_QT_PLUGIN_FILE}" ]; then
	wget ${LINUXDEPLOY_QT_PLUGIN_URL} -O "./Tools/${LINUXDEPLOY_QT_PLUGIN_FILE}"
fi
if [ ! -e ./Tools/linuxdeploy-update-plugin ]; then
	wget ${UPDATEPLUG_URL} -O ./Tools/linuxdeploy-update-plugin
fi
if [ ! -e ./Tools/appimageupdatetool ]; then
	wget ${UPDATETOOL_URL} -O ./Tools/appimageupdatetool
fi

chmod +x ./Tools/linuxdeploy
chmod +x ./Tools/linuxdeploy-update-plugin
chmod +x ./Tools/appimageupdatetool
chmod +x "./Tools/${LINUXDEPLOY_QT_PLUGIN_FILE}"

# Delete the AppDir folder to prevent build issues
rm -rf ./AppDir/

# Add the linux-env script to the AppDir prior to running linuxdeploy
mkdir -p ${APPDIR_HOOKS}
cp Data/linux-env.sh ${APPDIR_HOOKS}

# Ensure qt6 is properly set
qtchooser -install qt6 $(which qmake6)
export QT_SELECT=qt6

# Build the AppDir directory for this image
mkdir -p AppDir
./Tools/linuxdeploy \
	--appdir=./AppDir \
	-e ./build/Binaries/dolphin-emu \
	-d ./Data/slippi-dolphin.desktop \
	-i ./Data/dolphin-emu.png \
	--plugin qt

# Add the Sys dir to the AppDir for packaging
cp -r Data/Sys ${APPDIR_BIN}

# Build type
if [ "$1" == "playback" ] # Playback
    then
        echo "Using Playback build config"

		rm -f ${PLAYBACK_APPIMAGE_STRING}

		# Update Sys dir with playback codes
		echo "Copying Playback gecko codes"
		rm -rf "${APPDIR_BIN}/Sys/GameSettings" # Delete netplay codes
		cp -r "${PLAYBACK_CODES_PATH}/." "${APPDIR_BIN}/Sys/GameSettings/"

		OUTPUT="${PLAYBACK_APPIMAGE_STRING}" \
		./Tools/linuxdeploy-update-plugin --appdir=./AppDir/
else
		echo "Using Netplay build config"

		# remove existing appimage just in case
		rm -f ${NETPLAY_APPIMAGE_STRING}

		# Package up the update tool within the AppImage
		cp ./Tools/appimageupdatetool ./AppDir/usr/bin/

		# Bake an AppImage with the update metadata
		OUTPUT="${NETPLAY_APPIMAGE_STRING}" \
		./Tools/linuxdeploy-update-plugin --appdir=./AppDir/
fi

unset NO_STRIP
