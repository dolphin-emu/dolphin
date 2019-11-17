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
3) run `cd build`
4) run `cmake .. -DLINUX_LOCAL_DEV=true`
5) run `make`
6) run `cp -r ../Data/Sys/ Binaries/`
7) run `touch Binaries/portable.txt`

##### Setting Version

Windows:
Version updating is changed inside of the [make-scmrev.js](Source/Core/Common/make_scmrev.h.js) file at Line 94

Linux:
Version updating is changed inside of the [CMakeLists.txt](CMakeLists.txt) file at Line 159

### License
Dolphin RNO is licensed under the [GNU General Public License v2.0](license.txt)
