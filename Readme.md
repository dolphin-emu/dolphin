# Dolphin MPN

Dolphin MPN is an emulator for running GameCube and Wii games on Windows, macOS, and Linux


## Info
This emulator bundled a modified version of Dolphin

## Installation
<sub>(note: ROMs will need to be downloaded separately)</sub>

## Compiling

Windows:
1) Install [Git](https://gitforwindows.org/) and [Visual Studio 2019](https://visualstudio.microsoft.com/downloads/) (community is fine) if you haven't already
1) Clone the Git Repo `git clone https://github.com/MarioPartyNetplay/dolphin-mpn --recursive`
2) Open the **Source/dolphin-emu.sln** project in Visual Studio 2019
3) Set Build Path to **Release** -> **x64**
4) Click Build
5: Open Dolphin
**Dolphin is Located at Binaries/x64/**

Linux:
1) Open Terminal
2) Install Required Packages: 
   **Ubuntu** `sudo apt install curl ffmpeg git libao-dev libasound-dev libavcodec-dev libavformat-dev libbluetooth-dev libevdev-dev libgtk2.0-dev libhidapi-dev libmbedtls-dev libminiupnpc-dev libopenal-dev libpangocairo-1.0-0 libpulse-dev libsfml-dev libswscale-dev libudev-dev libusb-1.0-0-dev libxrandr-dev qt5-default qtbase5-private-dev`
3) Clone the Git Repo: `git clone https://github.com/MarioPartyNetplay/dolphin-mpn`
4) Enter the Cloned Repo: `cd dolphin-mpn`
5) Make the Build Directory: `mkdir build && cd build`
6) Setup CMake: `cmake .. -G Ninja -DLINUX_LOCAL_DEV=true`
7) Build Dolphin `ninja`
8) Copy Important Files `cp -r ../Data/Sys/ Binaries/ && cp -r ../Data/User/ Binaries/ && cp -r ../Data/dolphin-emu.png Binaries/ && touch Binaries/portable.txt`
9) Run Dolphin
**Dolphin is Located at build/Binaries/dolphin-emu**

macOS:
1) Install [Xcode Command Line Tools](https://developer.apple.com/xcode/features/) & [Brew](https://brew.sh/) if you havent
2) Open a Terminal
3) Install Brew: `brew install cmake qt5 ninja pkgconfig git`
4) Clone the Git Repo: `git clone https://github.com/MarioPartyNetplay/dolphin-mpn`
5) Enter the Cloned Repo: `cd dolphin-mpn`
6) Make the Build Directory: `mkdir -p build && cd build`
7) Setup CMake: `cmake .. -G Ninja -DCMAKE_CXX_FLAGS="-Xclang -fcolor-diagnostics" -DCMAKE_PREFIX_PATH=$(brew --prefix qt5)`
8) Build Dolphin: `ninja`
9) Copy Important Files: `cp -r ../Data/Sys/ Binaries/ && cp -r ../Data/User/ Binaries/ && cp -r ../Data/dolphin-emu.png Binaries/ && touch Binaries/portable.txt`
10) Run Dolphin
**Dolphin is Located at build/Binaries/dolphin-emu**

### License
Dolphin MPN is licensed under the [GNU General Public License v2.0](license.txt)
