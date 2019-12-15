# Dolphin RNO

Dolphin RNO is an emulator for running GameCube and Wii games on Windows and Linux 


## Info
This emulator bundled a modified version of Dolphin

## Installation
<sub>(note: ROMs will need to be downloaded separately)</sub>

## Compiling

Windows: 
1) Clone the Git Repo
2) Open Source/dolphin-emu.sln in Visual Studio 2017
3) Set Build Path to Release -> x64
4) Build

Linux:
1) Clone the Git Repo
2) Open Terminal inside of Git Repo
3) run `mkdir build`
4) run `cd build`
5) run `cmake .. -DLINUX_LOCAL_DEV=true`
6) run `make`
7) run `cp -r ../Data/Sys/ Binaries/`
8) run `touch Binaries/portable.txt`

macOS:
1) Install Xcode Command Line Tools if you havent
2) Open a Terminal
3) run `brew install cmake`
4) run `brew install qt5`
5) run `brew install ninja`
6) run `brew install pkgconfig`
6) Clone the Git Repo
7) run `mkdir -p build`
8) run `cd build`
9) run `cmake .. -DCMAKE_PREFIX_PATH=~/Qt/5.12.1/clang_64/ -G Ninja -DCMAKE_CXX_FLAGS="-Xclang -fcolor-diagnostics" -DCMAKE_PREFIX_PATH=$(brew --prefix qt5)`
10) run `ninja`

1) Clone the Git Repo
##### Setting Version

Windows:
Version updating is changed inside of the [make-scmrev.js](Source/Core/Common/make_scmrev.h.js) file at Line 94

Linux / macOS? :
Version updating is changed inside of the [CMakeLists.txt](CMakeLists.txt) file at Line 159

### License
Dolphin RNO is licensed under the [GNU General Public License v2.0](license.txt)
