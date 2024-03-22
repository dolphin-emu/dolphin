#!/bin/bash -e
# build-mac.sh

QT_BREW_PATH=$(brew --prefix qt@6)
CMAKE_FLAGS="-DQT_DIR=${QT_BREW_PATH}/lib/cmake/Qt6 -DENABLE_NOGUI=false"

export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/lib:/usr/lib/

# Build type
if [ "$1" == "playback" ]
then
        echo "Using Playback build config"
else
        echo "Using Netplay build config"
        CMAKE_FLAGS+=" -DSLIPPI_PLAYBACK=false"
fi

if [ "$CI" == "true" ]
then
        CMAKE_FLAGS+=" -DMACOS_CODE_SIGNING=OFF"
fi

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
cmake --build . --target dolphin-emu -- -j$(sysctl -n hw.ncpu)
popd
