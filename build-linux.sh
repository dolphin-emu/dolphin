#!/bin/bash -e
# build-linux.sh

CMAKE_FLAGS='-DLINUX_LOCAL_DEV=true'

# Build type
if [ -z "$1" ]
    then
        # TODO: move this around, playback should be the secondary build
        CMAKE_FLAGS+=" -DSLIPPI_PLAYBACK=false"
        echo "Using Netplay build config"
elif [ "$1" == "playback" ]
    then
        echo "Using Playback build config"
fi

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ../
make -j$(nproc)
popd

# Copy the Sys folder in
cp -r -n Data/Sys/ build/Binaries/

touch ./build/Binaries/portable.txt
