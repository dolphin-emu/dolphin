#!/bin/bash -e
# build-mac.sh

QT_BREW_PATH=$(brew --prefix qt@6)
CMAKE_FLAGS="-DQT_DIR=${QT_BREW_PATH}/lib/cmake/Qt6 -DENABLE_NOGUI=false"

# For some reason the system xxhash library doesn't get properly linked,
# at least on my M1. The clang command gets -lxxhash, but probably needs
# -L/opt/homebrew/lib/ to actually find the library.
if [[ $(arch) == 'arm64' ]]; then
  CMAKE_FLAGS+=" -DUSE_SYSTEM_XXHASH=OFF"
fi
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
