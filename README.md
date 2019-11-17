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
run `cp -r ../Data/Sys/ Binaries/`
run `touch Binaries/portable.txt`

##### Version

### License
Dolphin RNO is licensed under the [GNU General Public License v2.0](license.txt)
