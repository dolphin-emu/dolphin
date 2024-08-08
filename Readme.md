# Dolphin - A GameCube and Wii Emulator

[Homepage](https://dolphin-emu.org/) | [Project Site](https://github.com/dolphin-emu/dolphin) | [Buildbot](https://dolphin.ci/) | [Forums](https://forums.dolphin-emu.org/) | [Wiki](https://wiki.dolphin-emu.org/) | [GitHub Wiki](https://github.com/dolphin-emu/dolphin/wiki) | [Issue Tracker](https://bugs.dolphin-emu.org/projects/emulator/issues) | [Coding Style](https://github.com/dolphin-emu/dolphin/blob/master/Contributing.md) | [Transifex Page](https://app.transifex.com/dolphinemu/dolphin-emu/dashboard/)

Dolphin is an emulator for running GameCube and Wii games on Windows,
Linux, macOS, and recent Android devices. It's licensed under the terms
of the GNU General Public License, version 2 or later (GPLv2+).

Please read the [FAQ](https://dolphin-emu.org/docs/faq/) before using Dolphin.

## System Requirements

### Desktop

* OS
    * Windows (10 or higher).
    * Linux.
    * macOS (10.15 Catalina or higher).
    * Unix-like systems other than Linux are not officially supported, but might work.
* Processor
    * A CPU with SSE2 support.
    * A modern CPU (3 GHz and Dual Core, not older than 2008) is highly recommended.
* Graphics
    * A reasonably modern graphics card (Direct3D 11.1 / OpenGL 3.3).
    * A graphics card that supports Direct3D 11.1 / OpenGL 4.4 is recommended.

### Android

* OS
    * Android (5.0 Lollipop or higher).
* Processor
    * A processor with support for 64-bit applications (either ARMv8 or x86-64).
* Graphics
    * A graphics processor that supports OpenGL ES 3.0 or higher. Performance varies heavily with [driver quality](https://dolphin-emu.org/blog/2013/09/26/dolphin-emulator-and-opengl-drivers-hall-fameshame/).
    * A graphics processor that supports standard desktop OpenGL features is recommended for best performance.

Dolphin can only be installed on devices that satisfy the above requirements. Attempting to install on an unsupported device will fail and display an error message.

## Building
Compilation is split into several steps, with minor differences between
operating systems.

### Fetch
Download Dolphin's source code, enter the newly downloaded source tree, and
download the embedded dependencies as git submodules:
```sh
git clone --depth 1 https://github.com/dolphin-emu/dolphin &&
  cd dolphin &&
  git submodule update --depth 1 --init --recursive
```
In the example, only the latest commit is downloaded to speed up the process. If
the complete git history is needed, remove `--depth 1` from the commands. To
download the complete history after a shallow download, run the following
command from the project's directory:
```sh
git fetch --unshallow
```

### Compile
#### Microsoft Windows
Use the solution file `Source/dolphin-emu.sln` to build Dolphin on Windows.
Dolphin targets the latest MSVC shipped with Visual Studio or Build Tools.
Other compilers might be able to build Dolphin on Windows but have not been
tested and are not recommended to be used. Git and the latest Windows SDK must
be installed when building.

The "Release" solution configuration includes performance optimizations for the
best user experience, but complicates debugging Dolphin. The "Debug" solution
configuration is significantly slower, more verbose and less permissive, but
makes debugging Dolphin easier.

#### Linux, macOS, FreeBSD, and other Unix or Unix-like operating systems.
Dolphin requires [CMake](https://cmake.org/) for systems other than Windows.
You need a recent version of GCC or Clang with decent C++20 support. CMake will
inform you if your compiler is too old. Many libraries are bundled with Dolphin
and used if they're not installed on your system. CMake will inform you if a
bundled library is used or if you need to install any missing packages yourself.
You may refer to the
[wiki](https://github.com/dolphin-emu/dolphin/wiki/Building-for-Linux) for more
information.

Additionally, [Ninja](https://ninja-build.org/) is recommended for faster
compilation times.

From inside the checked-out source tree, run:
```sh
cmake -G Ninja -B build &&
  cmake --build build
```

Binaries will be available in `build/Binaries`. On macOS, an application bundle
will be created in the same directory.

#### Android
These instructions assume familiarity with Android development. If you do not
have an Android dev environment set up, see [AndroidSetup.md](AndroidSetup.md).

If using Android Studio, import the Gradle project located at
`./Source/Android`.

Android apps are compiled using a build system called Gradle. Dolphin's native
component, however, is compiled using CMake. The Gradle script will attempt to
run a CMake build automatically while building the Java code.

### Extra steps
#### Installation via CMake
To install Dolphin to your system:
```sh
cmake --install build
```

#### CMake local build
Pass `-DLINUX_LOCAL_DEV=true` to CMake when creating the build directory, and
put a symlink to Dolphin's assets in the same directory as the binaries:
```sh
cmake -DLINUX_LOCAL_DEV=true -G Ninja -B build &&
  cmake --build build &&
  ln -s Data/Sys build/Binaries/
```
Useful for development, as root access is not required.

#### CMake portable build
Pass `-DLINUX_LOCAL_DEV=true` to CMake when creating the build directory,
copypaste Dolphin's assets into the same directory as the binaries, and create a
`portable.txt` file in the same directory as the binaries:
```sh
cmake -DLINUX_LOCAL_DEV=true -G Ninja -B build &&
  cmake --build build &&
  cp -r Data/Sys build/Binaries/ &&
  touch build/Binaries/portable.txt
```
`build/Binaries` can be stored on external storage and used on different
systems. Also useful for having multiple distinct Dolphin setups for testing,
development, or TAS.

#### macOS universal binaries
Doing this is more complex as it requires installation of library dependencies
for both x64 and ARM (or universal library equivalents) and may require
specifying additional arguments to point to relevant library locations.
Run the following command for more details:
```sh
BuildMacOSUniversalBinary.py --help
```
To build universal binaries supporting both x64 and ARM in the same application
bundle, run the following command:
```sh
mkdir build &&
  (cd build && python ../BuildMacOSUniversalBinary.py)
```
Universal binaries will be available in the `build/universal` folder.

### Uninstalling
#### Microsoft Windows uninstall
On Windows, simply remove the extracted directory, unless it was installed with
the NSIS installer, in which case you can uninstall Dolphin like any other
Windows application.

#### macOS
macOS users can delete Dolphin.app to uninstall it.

#### Linux, FreeBSD, and other Unix or Unix-like operating systems
To uninstall Dolphin on other systems, run the following command as root:
```sh
cat install_manifest.txt | tr -d '\n' | xargs rm
```
Additionally, you'll want to remove the global user directory if you don't plan
on reinstalling Dolphin.

## Command Line Usage

```
Usage: Dolphin.exe [options]... [FILE]...

Options:
  --version             show program's version number and exit
  -h, --help            show this help message and exit
  -u USER, --user=USER  User folder path
  -m MOVIE, --movie=MOVIE
                        Play a movie file
  -e <file>, --exec=<file>
                        Load the specified file
  -n <16-character ASCII title ID>, --nand_title=<16-character ASCII title ID>
                        Launch a NAND title
  -C <System>.<Section>.<Key>=<Value>, --config=<System>.<Section>.<Key>=<Value>
                        Set a configuration option
  -s <file>, --save_state=<file>
                        Load the initial save state
  -d, --debugger        Show the debugger pane and additional View menu options
  -l, --logger          Open the logger
  -b, --batch           Run Dolphin without the user interface (Requires
                        --exec or --nand-title)
  -c, --confirm         Set Confirm on Stop
  -v VIDEO_BACKEND, --video_backend=VIDEO_BACKEND
                        Specify a video backend
  -a AUDIO_EMULATION, --audio_emulation=AUDIO_EMULATION
                        Choose audio emulation from [HLE|LLE]
```

Available DSP emulation engines are HLE (High Level Emulation) and
LLE (Low Level Emulation). HLE is faster but less accurate whereas
LLE is slower but close to perfect. Note that LLE has two submodes (Interpreter and Recompiler)
but they cannot be selected from the command line.

Available video backends are "D3D" and "D3D12" (they are only available on Windows), "OGL", and "Vulkan".
There's also "Null", which will not render anything, and
"Software Renderer", which uses the CPU for rendering and
is intended for debugging purposes only.

## DolphinTool Usage
```
usage: dolphin-tool COMMAND -h

commands supported: [convert, verify, header, extract]
```

```
Usage: convert [options]... [FILE]...

Options:
  -h, --help            show this help message and exit
  -u USER, --user=USER  User folder path, required for temporary processing
                        files.Will be automatically created if this option is
                        not set.
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -o FILE, --output=FILE
                        Path to the destination FILE.
  -f FORMAT, --format=FORMAT
                        Container format to use. Default is RVZ. [iso|gcz|wia|rvz]
  -s, --scrub           Scrub junk data as part of conversion.
  -b BLOCK_SIZE, --block_size=BLOCK_SIZE
                        Block size for GCZ/WIA/RVZ formats, as an integer.
                        Suggested value for RVZ: 131072 (128 KiB)
  -c COMPRESSION, --compression=COMPRESSION
                        Compression method to use when converting to WIA/RVZ.
                        Suggested value for RVZ: zstd [none|zstd|bzip|lzma|lzma2]
  -l COMPRESSION_LEVEL, --compression_level=COMPRESSION_LEVEL
                        Level of compression for the selected method. Ignored
                        if 'none'. Suggested value for zstd: 5
```

```
Usage: verify [options]...

Options:
  -h, --help            show this help message and exit
  -u USER, --user=USER  User folder path, required for temporary processing
                        files.Will be automatically created if this option is
                        not set.
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -a ALGORITHM, --algorithm=ALGORITHM
                        Optional. Compute and print the digest using the
                        selected algorithm, then exit. [crc32|md5|sha1]
```

```
Usage: header [options]...

Options:
  -h, --help            show this help message and exit
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -b, --block_size      Optional. Print the block size of GCZ/WIA/RVZ formats,
then exit.
  -c, --compression     Optional. Print the compression method of GCZ/WIA/RVZ
                        formats, then exit.
  -l, --compression_level
                        Optional. Print the level of compression for WIA/RVZ
                        formats, then exit.
```

```
Usage: extract [options]...

Options:
  -h, --help            show this help message and exit
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -o FOLDER, --output=FOLDER
                        Path to the destination FOLDER.
  -p PARTITION, --partition=PARTITION
                        Which specific partition you want to extract.
  -s SINGLE, --single=SINGLE
                        Which specific file/directory you want to extract.
  -l, --list            List all files in volume/partition. Will print the
                        directory/file specified with --single if defined.
  -q, --quiet           Mute all messages except for errors.
  -g, --gameonly        Only extracts the DATA partition.
```
