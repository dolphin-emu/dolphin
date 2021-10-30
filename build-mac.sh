#!/bin/bash -e
# build-mac.sh

QT_BREW_PATH=$(brew --prefix qt@5)
CMAKE_FLAGS="-DQt5_DIR=${QT_BREW_PATH}/lib/cmake/Qt5 -DENABLE_NOGUI=false"

PLAYBACK_CODES_PATH="./Data/PlaybackGeckoCodes/"

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/Slippi_Dolphin.app/Contents/Resources/"

export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/lib:/usr/lib/

# Build type
if [ "$1" == "playback" ]
    then
        echo "Using Playback build config"
else
        echo "Using Netplay build config"
        CMAKE_FLAGS+=" -DSLIPPI_PLAYBACK=false"
fi

if [[ -z "${CERTIFICATE_MACOS_APPLICATION}" ]]
    then
        echo "Building without code signing"
else
        echo "Building with code signing"
        CMAKE_FLAGS+=' -DMACOS_CODE_SIGNING="ON"'
fi

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
make -j7
popd

# Copy the Sys folder in
echo "Copying Sys files into the bundle"
cp -Rfn "${DATA_SYS_PATH}" "${BINARY_PATH}"

# Copy playback specific codes if needed
if [ "$1" == "playback" ]
    then
        # Update Sys dir with playback codes
        echo "Copying playback gecko codes into the bundle"
		rm -rf "${BINARY_PATH}/Sys/GameSettings" # Delete netplay codes
		cp -r "${PLAYBACK_CODES_PATH}/." "${BINARY_PATH}/Sys/GameSettings/"
fi
