# Discord RPC

This is a library for interfacing your game with a locally running Discord desktop client. It's known to work on Windows, macOS, and Linux. You can use the lib directly if you like, or use it as a guide to writing your own if it doesn't suit your game as is. PRs/feedback welcome if you have an improvement everyone might want, or can describe how this doesn't meet your needs.

Included here are some quick demos that implement the very minimal subset to show current status, and
have callbacks for where a more complete game would do more things (joining, spectating, etc).

## Documentation

The most up to date documentation for Rich Presence can always be found on our [developer site](https://discordapp.com/developers/docs/rich-presence/how-to)! If you're interested in rolling your own native implementation of Rich Presence via IPC sockets instead of using our SDK—hey, you've got free time, right?—check out the ["Hard Mode" documentation](https://github.com/discordapp/discord-rpc/blob/master/documentation/hard-mode.md).

## Basic Usage

Zeroith, you should be set up to build things because you are a game developer, right?

First, head on over to the [Discord developers site](https://discordapp.com/developers/applications/me) and make yourself an app. Keep track of `Client ID` -- you'll need it here to pass to the init function.

### Unity Setup

If you're a Unity developer looking to integrate Rich Presence into your game, follow this simple guide to get started towards success:

1. Download the DLLs for any platform that you need from [our releases](https://github.com/discordapp/discord-rpc/releases)
2. In your Unity project, create a `Plugins` folder inside your `Assets` folder if you don't already have one
3. Copy the file `DiscordRpc.cs` from [here](https://github.com/discordapp/discord-rpc/blob/master/examples/button-clicker/Assets/DiscordRpc.cs) into your `Assets` folder. This is basically your header file for the SDK

We've got our `Plugins` folder ready, so let's get platform-specific!

#### Windows

4. Create `x86` and `x86_64` folders inside `Assets/Plugins/`
5. Copy `discord-rpc-win/win64-dynamic/bin/discord-rpc.dll` to `Assets/Plugins/x86_64/`
6. Copy `discord-rpc-win/win32-dynamic/bin/discord-rpc.dll` to `Assets/Plguins/x86/`
7. Click on both DLLs and make sure they are targetting the correct architectures in the Unity editor properties pane
8. Done!

#### MacOS

4. Copy `discord-rpc-osx/osx-dynamic/lib/libdiscord-rpc.dylib` to `Assets/Plugins/`
5. Rename `libdiscord-rpc.dylib` to `libdiscord-rpc.bundle`
6. Done!

#### Linux

4. Copy `discord-rpc-linux/linux-dynamic-lib/libdiscord-rpc.so` to `Assets/Plugins/`
5. Done!

You're ready to roll! For code examples on how to interact with the SDK using the `DiscordRpc.cs` header file, check out [our example](https://github.com/discordapp/discord-rpc/blob/master/examples/button-clicker/Assets/DiscordController.cs)

### From package

Download a release package for your platform(s) -- they have subdirs with various prebuilt options, select the one you need add `/include` to your compile includes, `/lib` to your linker paths, and link with `discord-rpc`. For the dynamically linked builds, you'll need to ship the associated file along with your game.

### From repo

First-eth, you'll want `CMake`. There's a few different ways to install it on your system, and you should refer to [their website](https://cmake.org/install/). Many package managers provide ways of installing CMake as well.

To make sure it's installed correctly, type `cmake --version` into your flavor of terminal/cmd. If you get a response with a version number, you're good to go!

There's a [CMake](https://cmake.org/download/) file that should be able to generate the lib for you; Sometimes I use it like this:

```sh
    cd <path to discord-rpc>
    mkdir build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=<path to install discord-rpc to>
    cmake --build . --config Release --target install
```

There is a wrapper build script `build.py` that runs `cmake` with a few different options.

Usually, I run `build.py` to get things started, then use the generated project files as I work on things. It does depend on `click` library, so do a quick `pip install click` to make sure you have it if you want to run `build.py`.

There are some CMake options you might care about:

| flag                                                                                     | default | does                                                                                                                                                  |
| ---------------------------------------------------------------------------------------- | ------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ENABLE_IO_THREAD`                                                                       | `ON`    | When enabled, we start up a thread to do io processing, if disabled you should call `Discord_UpdateConnection` yourself.                              |
| `USE_STATIC_CRT`                                                                         | `OFF`   | (Windows) Enable to statically link the CRT, avoiding requiring users install the redistributable package. (The prebuilt binaries enable this option) |
| [`BUILD_SHARED_LIBS`](https://cmake.org/cmake/help/v3.7/variable/BUILD_SHARED_LIBS.html) | `OFF`   | Build library as a DLL                                                                                                                                |
| `WARNINGS_AS_ERRORS`                                                                     | `OFF`   | When enabled, compiles with `-Werror` (on \*nix platforms).                                                                                           |

## Continuous Builds

Why do we have three of these? Three times the fun!

| CI                   | badge                                                                                                                                            |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| TravisCI             | [![Build status](https://travis-ci.org/discordapp/discord-rpc.svg?branch=master)](https://travis-ci.org/discordapp/discord-rpc)                  |
| AppVeyor             | [![Build status](https://ci.appveyor.com/api/projects/status/qvkoc0w1c4f4b8tj?svg=true)](https://ci.appveyor.com/project/crmarsh/discord-rpc)    |
| Buildkite (internal) | [![Build status](https://badge.buildkite.com/e103d79d247f6776605a15246352a04b8fd83d69211b836111.svg)](https://buildkite.com/discord/discord-rpc) |

## Sample: send-presence

This is a text adventure "game" that inits/deinits the connection to Discord, and sends a presence update on each command.

## Sample: button-clicker

This is a sample [Unity](https://unity3d.com/) project that wraps a DLL version of the library, and sends presence updates when you click on a button. Run `python build.py unity` in the root directory to build the correct library files and place them in their respective folders.

## Sample: unrealstatus

This is a sample [Unreal](https://www.unrealengine.com) project that wraps the DLL version of the library with an Unreal plugin, exposes a blueprint class for interacting with it, and uses that to make a very simple UI. Run `python build.py unreal` in the root directory to build the correct library files and place them in their respective folders.

### Using the Unreal Engine plugin with your own project

To use the Rich Presense plugin with Unreal Engine Projects:

1.  Download the latest [release](https://github.com/discordapp/discord-rpc/releases) for each operating system you are targeting and the zipped source code
2.  In the source code zip, copy the UE plugin—`examples/unrealstatus/Plugins/discordrpc`—to your project's plugin directory
3.  At `[YOUR_UE_PROJECT]/Plugins/discordrpc/source/ThirdParty/DiscordRpcLibrary/`, create an `Include` folder and copy `discord_rpc.h` and `discord_register.h` to it from the zip
4.  Follow the steps below for each OS
5.  Build your UE4 project
6.  Launch the editor, and enable the Discord plugin.

#### Windows

- At `[YOUR_UE_PROJECT]/Plugins/discordrpc/source/ThirdParty/DiscordRpcLibrary/`, create a `Win64` folder
- Copy `lib/discord-rpc.lib` and `bin/discord-rpc.dll` from `[RELEASE_ZIP]/win64-dynamic` to the `Win64` folder

#### Mac

- At `[YOUR_UE_PROJECT]/Plugins/discordrpc/source/ThirdParty/DiscordRpcLibrary/`, create a `Mac` folder
- Copy `libdiscord-rpc.dylib` from `[RELEASE_ZIP]/osx-dynamic/lib` to the `Mac` folder

#### Linux

- At `[YOUR_UE_PROJECT]/Plugins/discordrpc/source/ThirdParty/DiscordRpcLibrary/`, create a `Linux` folder
- Inside, create another folder `x86_64-unknown-linux-gnu`
- Copy `libdiscord-rpc.so` from `[RELEASE_ZIP]/linux-dynamic/lib` to `Linux/x86_64-unknown-linux-gnu`

## Wrappers and Implementations

Below is a table of unofficial, community-developed wrappers for and implementations of Rich Presence in various languages. If you would like to have yours added, please make a pull request adding your repository to the table. The repository should include:

- The code
- A brief ReadMe of how to use it
- A working example

###### Rich Presence Wrappers and Implementations

| Name                                                                      | Language                          |
| ------------------------------------------------------------------------- | --------------------------------- |
| [Discord RPC C#](https://github.com/Lachee/discord-rpc-csharp)            | C#                                |
| [Discord RPC D](https://github.com/voidblaster/discord-rpc-d)             | [D](https://dlang.org/)           |
| [discord-rpc.jar](https://github.com/Vatuu/discord-rpc 'Discord-RPC.jar') | Java                              |
| [java-discord-rpc](https://github.com/MinnDevelopment/java-discord-rpc)   | Java                              |
| [Discord-IPC](https://github.com/jagrosh/DiscordIPC)                      | Java                              |
| [Discord Rich Presence](https://npmjs.org/discord-rich-presence)          | JavaScript                        |
| [drpc4k](https://github.com/Bluexin/drpc4k)                               | [Kotlin](https://kotlinlang.org/) |
| [lua-discordRPC](https://github.com/pfirsich/lua-discordRPC)              | LuaJIT (FFI)                      |
| [pypresence](https://github.com/qwertyquerty/pypresence)                  | [Python](https://python.org/)     |
| [SwordRPC](https://github.com/Azoy/SwordRPC)                              | [Swift](https://swift.org)        |
