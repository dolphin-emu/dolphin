
## Prepare

Update `Source/Core/DolphinUIKit/Main.mm` to point to your ISO.

## Build

To build:

    $ cmake -Bbuild -H. -GNinja -DCMAKE_SYSTEM_NAME=Darwin -DCMAKE_CROSSCOMPILING=TRUE -DIPHONEOS=TRUE
    $ ninja -C build

Note that device and simulator builds each require extra CMake options.

## Simulator

To build:

    $ cmake [...] -DCMAKE_SYSTEM_PROCESSOR=x86_64

To install:

    $ simctl install booted build/Binaries/Dolphin.app/

To launch without a debugger:

    $ simctl launch booted org.dolphin-emu.dolphin

To launch and attach LLDB:

    $ simctl launch -w booted org.dolphin-emu.dolphin
    $ lldb
    (lldb) attach <pid>
    (lldb) continue

## Device

To build you will also need to specify a code signing identity and team identifier:

    $ cmake [...] -DCMAKE_SYSTEM_PROCESSOR=arm64 -DIPHONEOS_CODE_SIGNING_IDENTITY=<identity> -DIPHONEOS_TEAM_IDENTIFIER=<identifier>

To install using [ios-deploy](https://github.com/phonegap/ios-deploy):

    $ ios-deploy -b build/Binaries/Dolphin.app/

To install and attach a debugger:

    $ ios-deploy -d -b build/Binaries/Dolphin.app/

