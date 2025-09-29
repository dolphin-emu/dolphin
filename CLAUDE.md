# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.


## Architecture

### Core Components

#### Source/Core/
- **Core/**: Main emulation core, CPU/GPU emulation, timing, system state
  - `DSP/`: Digital Signal Processor emulation (both HLE and LLE modes)
  - `PowerPC/`: PowerPC processor emulation
  - `HW/`: Hardware component emulation (controllers, memory cards, etc.)
  - `IOS/`: Wii's IOS (Starlet ARM processor) emulation
  - `Boot/`: System boot process handling
- **Common/**: Shared utilities, logging, file I/O, cross-platform abstractions
- **AudioCommon/**: Audio processing and output management 
- **VideoCommon/**: Graphics backend abstraction layer
- **VideoBackends/**: Platform-specific graphics backends (D3D11/12, OpenGL, Vulkan, Metal)
- **DiscIO/**: Game disc/image file parsing and loading
- **InputCommon/**: Input device handling and controller mapping

#### User Interfaces
- **DolphinQt/**: Qt-based GUI (primary desktop interface)
- **DolphinNoGUI/**: Command-line interface for headless operation
- **DolphinTool/**: Disc image conversion and manipulation utility

#### Platform-Specific
- **Android/**: Android app implementation
- **MacUpdater/**: macOS-specific updater functionality


- Implementing DPL2 in Sound Engine: @DPL2.md
### Emulation Architecture

#### DSP Emulation Modes
- **HLE (High Level Emulation)**: Fast, less accurate, emulates DSP at API level. 
- **LLE (Low Level Emulation)**: Slower, highly accurate, emulates actual DSP code
  - LLE has Interpreter and Recompiler submodes

#### Implementing Dolby Pro Logic II (DPL2) in Sound Engine
- Full documentation in @DPL2.md

#### Video Backend System
- Abstracted through VideoCommon to support multiple graphics APIs
- Available backends: D3D11, D3D12 (Windows), OpenGL, Vulkan, Metal (macOS), Software Renderer, Null

#### Memory Management
- Virtual memory mapping for GameCube/Wii address spaces
- JIT compilation for PowerPC code translation
- Save state system for full emulation state preservation

### Key Subsystems
- **NetPlay**: Online multiplayer functionality with client/server architecture
- **Achievement System**: RetroAchievements integration
- **Movie System**: Input recording/playback for TAS (Tool-Assisted Speedrun)
- **Debugger**: Real-time debugging tools for emulated system
- **Config System**: Hierarchical configuration management
- **Patch Engine**: Game-specific fixes and enhancements

## Development Notes

### Code Organization
- Most core emulation logic is in `Source/Core/Core/`
- Platform abstractions in `Source/Core/Common/`
- UI code separated by framework (Qt in `DolphinQt/`)
- Tests mirror source structure under `Source/UnitTests/`


## Build Commands

### Windows (Visual Studio)
- **Build**: Use `"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Source/dolphin-emu.sln /p:Configuration=Release /p:Platform=x64 /m` for release builds
- **Debug Build**: Use `"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Source/dolphin-emu.sln /p:Configuration=Debug /p:Platform=x64 /m` for debug builds
- **Project File**: `Source/dolphin-emu.sln` is the main solution file
- **Prerequisites**: Git and latest Windows SDK must be installed

### Linux/macOS (CMake)
- **Configure**: `mkdir build && cd build && cmake ..`
- **Build**: `make -j $(nproc)` on Linux, `make -j $(sysctl -n hw.logicalcpu)` on macOS
- **Local Development**: Use `cmake .. -DLINUX_LOCAL_DEV=true` and `ln -s ../../Data/Sys Binaries/`

### Android
- **Location**: `Source/Android/` contains the Gradle project
- **Build System**: Gradle with CMake for native components

### Common Prerequisites
- **Submodules**: Always run `git submodule update --init --recursive` before building
- **Compiler Requirements**: C++20 support required (latest MSVC on Windows, recent GCC/Clang on Linux/macOS)

## Testing

### Unit Tests
- **Run Tests**: `ctest --output-on-failure` or build the `unittests` target
- **Test Executable**: Built as `tests` in `Tests/` subdirectory
- **Framework**: Uses Google Test (gtest)
- **Individual Tests**: Test files are organized under `Source/UnitTests/`

### Test Structure
- Tests are built as object libraries and linked to main test executable
- Test data copied from `Data/Sys/` during build


### Build System
- Windows: Visual Studio solution files (.sln/.vcxproj)
- Other platforms: CMake with extensive external dependency management
- Supports both system and bundled libraries
- Claude is running in a MingW64 environment so unix path styles must be used to invoke MSBuild. The C drive is located at "/C" instead of "C:". The full command line is: 
"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" "Source/dolphin-emu.sln" -t:Build -p:Configuration=Release -p:Platform=x64 -m -nologo -v:m
- Do not build Dolpin in Debug Mode by default unless I ask you to do it
- To run the built binary, always run "/C/Users/gille/Dev/dolphin-pristine/Binary/x64/Dolphin.exe" in the background

## GameCube Audio System Architecture

Make sure to read the following articles about the Gamecube Audio:
- https://blog.lse.epita.fr/2012/12/03/emulating-the-gamecube-audio-processing-in-dolphin.html
- https://blog.delroth.net/2013/07/why-dolphin-is-getting-rid-of-asynchronous-audio-processing/ 
- https://pokeacer.xyz/wii/pdf/AX.pdf: this doc about the AX microde mentions Revolution (codename for Wii) but it applies equally to the GameCube which offers the same microcode. In Dolphin, the HLE implementation of AX for the Wii is AXWii.cpp and AX.cpp for the GameCube).



#### Hardware Components
- **Audio DSP**: 100 MHz dedicated Digital Signal Processor for audio processing
- **ARAM (Auxiliary RAM)**: 16 MB dedicated audio memory, accessible only via DMA
- **AI (Audio Interface)**: Converts processed digital audio to analog output signals

#### DSP Processing Pipeline
- **UCodes (Microcodes)**: Audio programs that run on the DSP
  - **AX UCode**: Most common, handles standard audio processing (Star Wars Rogue Squadron, Soul Calibur 2)
  - **Zelda UCode**: Specialized for Zelda games (Zelda the WindWaker, Super Mario Sunshine)
  - **JAC UCode**: Less common variant
- **Audio Processing**: 32 kHz mixing with 24-bit precision
- **Voice Processing Pipeline**: Sample reading → decompression → pitch adjustment → FIR filtering → volume ramping → mixing

#### Audio Mixing System
- The GameCube DSP uses a 3x3 symmetrical mixer with three buses:
  - **Main Bus**: Primary audio output
  - **AuxA Effects Bus**: First auxiliary effects channel
  - **AuxB Effects Bus**: Second auxiliary effects channel
Each bus supports Left, Right, and Surround channels. Every voice can send to Main/AuxA/AuxB with L/R/S components; the CPU can also read/modify the AUX buffers, then the DSP mixes AUX back with main.

The DSP can handle Volume/panning control, pitch modulation, initial time delay, environmental FIR filtering.

### Dolphin's Audio Emulation 

#### Design
Modern HLE implementation comments made by its author (Delroth) in Dolphin forums:

A DSP Thread is created when the UCode is loaded. This thread will be responsible for all the sound mixing work the DSP does. When we get commands from the CPU, we copy the command list to a temporary buffer, and wake up the DSP Thread to tell him we have commands to process. The DSP Thread handles the commands, sends a message to the CPU when it's done, and goes back to sleep.

It is basically the exact same model DSP LLE on Thread uses, with less synchronization (LLE tries to match the number of cycles executed on CPU and DSP, which causes some extra performance hit). This also kind of matches what happens on the real hardware, using 2 chips instead of 2 threads. However, this also means the audio processing speed is tied to the CPU speed: if the CPU cannot keep up, it won't send commands often enough and the audio backend won't receive enough data to avoid stuttering.

Another change, this time not exactly linked to overall design, is that the HLE now handles most AX commands. Some of the commands are used to set up global volume ramping, send data back to the main RAM, mix additional data from the RAM, or output samples to the buffers used by the audio interface. The HLE implementation follows the correct audio emulation pipeline: ARAM -> DSP -> RAM -> AI -> Output. This also means some CPU sound effects like echo, reverb, etc. should work fine.

- **Synchronous Processing**: Audio DSP runs synchronized with CPU to maintain:
  - Sub-5ms timing precision required by games
  - Proper audio effects (echo, reverb, chorus, delay)
  - Event triggering based on audio processing status
  - AUX processing functionality

- The Zelda UCode relies on a lot of CPU<->DSP sync via interrupts (can cause freezes with LLE on Thread) (source: Delroth on Dolphin forums)


