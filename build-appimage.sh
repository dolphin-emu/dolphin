#!/bin/bash -e
# build-online-appimage.sh

ZSYNC_STRING="gh-releases-zsync|project-slippi|Ishiiruka|latest|Slippi_Online-x86_64.AppImage.zsync"
PLAYBACK_APPIMAGE_STRING="Slippi_Playback-x86_64.AppImage"

LINUXDEPLOY_PATH="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous"
LINUXDEPLOY_FILE="linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_URL="${LINUXDEPLOY_PATH}/${LINUXDEPLOY_FILE}"

UPDATEPLUG_PATH="https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous"
UPDATEPLUG_FILE="linuxdeploy-plugin-appimage-x86_64.AppImage"
UPDATEPLUG_URL="${UPDATEPLUG_PATH}/${UPDATEPLUG_FILE}"

UPDATETOOL_PATH="https://github.com/AppImage/AppImageUpdate/releases/download/continuous"
UPDATETOOL_FILE="appimageupdatetool-x86_64.AppImage"
UPDATETOOL_URL="${UPDATETOOL_PATH}/${UPDATETOOL_FILE}"

DESKTOP_APP_URL="https://github.com/project-slippi/slippi-desktop-app"
DESKTOP_APP_SYS_PATH="./slippi-desktop-app/app/dolphin-dev/overwrite/Sys"

APPDIR_BIN="./AppDir/usr/bin"

# Grab various appimage binaries from GitHub if we don't have them
if [ ! -e ./Tools/linuxdeploy ]; then
	wget ${LINUXDEPLOY_URL} -O ./Tools/linuxdeploy
	chmod +x ./Tools/linuxdeploy
fi
if [ ! -e ./Tools/linuxdeploy-update-plugin ]; then
	wget ${UPDATEPLUG_URL} -O ./Tools/linuxdeploy-update-plugin
	chmod +x ./Tools/linuxdeploy-update-plugin
fi
if [ ! -e ./Tools/appimageupdatetool ]; then
	wget ${UPDATEPLUG_URL} -O ./Tools/appimageupdatetool
	chmod +x ./Tools/appimageupdatetool
fi

# Delete the AppDir folder to prevent build issues
rm -rf ./AppDir/

# Build the AppDir directory for this image
mkdir -p AppDir
./Tools/linuxdeploy \
	--appdir=./AppDir \
	-e ./build/Binaries/dolphin-emu \
	-d ./Data/slippi-online.desktop \
	-i ./Data/dolphin-emu.png

# Add the Sys dir to the AppDir for packaging
cp -r Data/Sys ${APPDIR_BIN}

# Build type
if [ -z "$1" ] # Netplay
    then
        echo "Using Netplay build config"
		
		# Package up the update tool within the AppImage
		cp ./Tools/appimageupdatetool ./AppDir/usr/bin/

		# Bake an AppImage with the update metadata
		UPDATE_INFORMATION="${ZSYNC_STRING}" \
			./Tools/linuxdeploy-update-plugin --appdir=./AppDir/
elif [ "$1" == "playback" ] # Playback
    then
        echo "Using Playback build config"
		if [ -d "slippi-desktop-app" ]
			then
				pushd slippi-desktop-app
				git checkout master
				git pull --ff-only
				popd
		else
			git clone ${DESKTOP_APP_URL}
		fi
		# Update Sys dir with playback codes
		rm -rf "${APPDIR_BIN}/GameSettings" # Delete netplay codes
		cp -r ${DESKTOP_APP_SYS_PATH} ${APPDIR_BIN}

		OUTPUT="${PLAYBACK_APPIMAGE_STRING}" \
		./Tools/linuxdeploy-update-plugin --appdir=./AppDir/
fi
