# Dolphin Emulator - Architecture Guide

A comprehensive map of the Dolphin codebase for developers and agents working on the project.

Dolphin is an open-source Nintendo GameCube and Wii emulator written primarily in C++. It supports Windows, Linux, macOS, and Android, with multiple graphics backends (OpenGL, Vulkan, Direct3D 11/12, Metal) and both interpreter and JIT-based CPU emulation.

---

## Table of Contents

1. [Repository Layout](#repository-layout)
2. [High-Level Architecture](#high-level-architecture)
3. [Core System Object](#core-system-object)
4. [CPU Emulation (PowerPC)](#cpu-emulation-powerpc)
5. [Memory System](#memory-system)
6. [Hardware Emulation](#hardware-emulation)
7. [GPU / Video Pipeline](#gpu--video-pipeline)
8. [Audio Subsystem](#audio-subsystem)
9. [Input Subsystem](#input-subsystem)
10. [Disc I/O](#disc-io)
11. [Wii-Specific: IOS Emulation](#wii-specific-ios-emulation)
12. [Frontend / UI Layer](#frontend--ui-layer)
13. [Networking and NetPlay](#networking-and-netplay)
14. [Common Utilities](#common-utilities)
15. [Build System](#build-system)
16. [Key Architectural Patterns](#key-architectural-patterns)
17. [Data Flow Diagrams](#data-flow-diagrams)
18. [File Quick-Reference](#file-quick-reference)

---

## Repository Layout

```
dolphin/
├── Source/                  # All source code
│   ├── Core/                # Main emulator code (see below)
│   └── Android/             # Android app (Kotlin/Java + JNI)
├── Externals/               # 50+ third-party dependencies (as submodules)
├── Data/                    # Runtime data files, system resources, icons
├── CMake/                   # CMake modules for finding/configuring libraries
├── docs/                    # Hardware documentation and specs
├── Tools/                   # Dev utilities (IDA/Ghidra scripts, perf tools)
├── Languages/               # Internationalization (.po translation files)
├── Installer/               # Platform-specific installer configs
├── Flatpak/                 # Linux Flatpak packaging
├── LICENSES/                # License files
├── CMakeLists.txt           # Root build configuration
└── dolphin-emu.sln          # Visual Studio solution
```

### Source/Core/ — The Heart of the Emulator

```
Source/Core/
├── Core/               # Emulation engine (CPU, HW, boot, IOS, debugger)
├── VideoCommon/         # Graphics abstraction layer and GX state emulation
├── VideoBackends/       # Rendering backend implementations
│   ├── OGL/             #   OpenGL 4.x
│   ├── Vulkan/          #   Vulkan
│   ├── D3D/             #   Direct3D 11
│   ├── D3D12/           #   Direct3D 12
│   ├── D3DCommon/       #   Shared D3D code
│   ├── Metal/           #   Metal (macOS/iOS)
│   ├── Software/        #   Software rasterizer (accuracy reference)
│   └── Null/            #   No-op backend (testing/benchmarking)
├── AudioCommon/         # Audio output abstraction and backends
├── InputCommon/         # Input device abstraction and controller emulation
├── DiscIO/              # Disc image parsing and filesystem access
├── Common/              # Shared utilities (threading, config, crypto, math)
├── UICommon/            # Shared UI helpers (game file, resource packs)
├── DolphinQt/           # Qt desktop GUI frontend
├── DolphinNoGUI/        # Headless/CLI frontend
├── DolphinTool/         # Command-line disc utility
├── MacUpdater/          # macOS auto-updater
├── WinUpdater/          # Windows auto-updater
└── UpdaterCommon/       # Shared update logic
```

---

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Frontend / UI Layer                        │
│    DolphinQt (Desktop)  │  DolphinNoGUI  │  Android App        │
└────────────────┬────────┴────────┬────────┴──────────┬──────────┘
                 │     Host Interface / JNI Bridge      │
                 └──────────────────┬───────────────────┘
┌────────────────────────────────────────────────────────────────┐
│                     Core::System (Singleton)                   │
│  Owns all emulation subsystems, provides unified access        │
├────────────┬──────────┬──────────┬──────────┬─────────────────┤
│  PowerPC   │  Memory  │ CoreTim- │  HW      │  IOS (Wii)      │
│  CPU       │  Manager │ ing      │  Devices │  Kernel          │
├────────────┴──────────┴──────────┴──────────┴─────────────────┤
│                    Video Pipeline                              │
│  VideoCommon (GX emulation) → AbstractGfx → Video Backends    │
├────────────────────────────────────────────────────────────────┤
│  AudioCommon      │  InputCommon        │  DiscIO              │
│  (Mixer+Backends) │  (Controllers+HID)  │  (Disc Formats)      │
├────────────────────────────────────────────────────────────────┤
│                    Common Utilities                             │
│  Config, Logging, FileUtil, Crypto, Threading, Math, etc.      │
└────────────────────────────────────────────────────────────────┘
```

---

## Core System Object

**File:** `Source/Core/Core/System.h`

`Core::System` is the central singleton that owns all emulation subsystems. It provides accessor methods for every major component and tracks global emulation state (GameCube vs Wii mode, dual-core mode, MMU mode, etc.).

Key accessors:

| Method | Returns | Purpose |
|--------|---------|---------|
| `GetPowerPC()` | `PowerPC::PowerPCManager&` | CPU emulation manager |
| `GetPPCState()` | `PowerPC::PowerPCState&` | CPU register state |
| `GetMMU()` | `PowerPC::MMU&` | Memory management unit |
| `GetMemory()` | `Memory::MemoryManager&` | Physical memory map |
| `GetCPU()` | `CPU::CPUManager&` | CPU thread control |
| `GetCoreTiming()` | `CoreTiming::CoreTimingManager&` | Event scheduler |
| `GetDSP()` | `DSP::DSPManager&` | Audio DSP |
| `GetDVDInterface()` | `DVD::DVDInterface&` | Disc drive |
| `GetSerialInterface()` | `SerialInterface::...Manager&` | Controllers (SI) |
| `GetExpansionInterface()` | `ExpansionInterface::...Manager&` | Memory cards, etc. (EXI) |
| `GetProcessorInterface()` | `ProcessorInterface::...Manager&` | Interrupt controller |
| `GetVideoInterface()` | `VideoInterface::...Manager&` | Video timing |
| `GetAudioInterface()` | `AudioInterface::...Manager&` | Audio streaming |
| `GetFifo()` | `Fifo::FifoManager&` | GPU command FIFO |
| `GetCommandProcessor()` | `CommandProcessor::...Manager&` | GPU command parser |
| `GetIOS()` | `IOS::HLE::EmulationKernel*` | Wii IOS kernel |
| `GetMovie()` | `Movie::MovieManager&` | TAS movie recording |
| `GetJitInterface()` | `JitInterface&` | JIT compiler control |

---

## CPU Emulation (PowerPC)

**Directory:** `Source/Core/Core/PowerPC/`

Dolphin emulates the IBM Gekko (GameCube) and Broadway (Wii) processors — both PowerPC 750-class CPUs with paired-singles SIMD extensions.

### CPU State (`PowerPC.h`)

The `PowerPCState` struct holds the full CPU register file, carefully laid out for JIT efficiency:

- **32 GPRs** (`gpr[32]`) — 32-bit general-purpose registers
- **32 FPRs** as paired singles (`ps[32]`) — each holds two 64-bit floats (PS0/PS1)
- **PC / NPC** — program counter / next program counter
- **CR** — condition register
- **MSR** — machine state register (interrupts, MMU, FP exceptions)
- **1024 SPRs** — link register, count register, BATs, time base, HID registers, etc.
- **TLB cache** — 128 entries x 2 ways for instruction and data
- **Segment registers** — 16 for virtual address translation

### Execution Engines

All engines implement `CPUCoreBase` (`CPUCoreBase.h`):

| Engine | ID | Description |
|--------|----|-------------|
| **Interpreter** | `CPUCore::Interpreter` | Pure software interpretation. Slowest but most accurate |
| **JIT64** | `CPUCore::JIT64` | x86-64 dynamic recompiler. Primary on desktop x86 |
| **JitArm64** | `CPUCore::JITARM64` | ARM64 dynamic recompiler. Primary on Apple Silicon / Android |
| **CachedInterpreter** | `CPUCore::CachedInterpreter` | Compiles blocks to sequences of interpreter function pointers |

### JIT Architecture (`JitCommon/`, `Jit64/`, `JitArm64/`)

The JIT compiles PowerPC basic blocks into native code on demand:

1. **Block discovery** — PowerPC instructions decoded until a branch
2. **Register allocation** — PowerPC registers cached in host registers (`RegCache/`)
3. **Code generation** — Native instructions emitted via `x64Emitter` or `Arm64Emitter`
4. **Block linking** — Compiled blocks linked directly for fast transitions
5. **Block cache** — Maps PowerPC addresses to compiled code; invalidated on code writes

Key JIT optimizations:
- **Fastmem** — Maps guest memory into host address space; faults caught by signal handler
- **Constant propagation** — Tracks known register values at compile time
- **Gather pipe batching** — FIFO writes coalesced for fewer flushes
- **BLR optimization** — Function return prediction
- **BAT caching** — Address translation results reused across instructions

### DSP (Digital Signal Processor)

**Directory:** `Source/Core/Core/DSP/`

The GameCube/Wii includes a dedicated DSP for audio mixing and effects. Dolphin supports two modes:

- **HLE (High-Level Emulation)** — Simulates DSP behavior by recognizing known microcode (uCode) patterns (AX, AXWii, Zelda, etc.). Faster but less accurate. Located in `Source/Core/Core/HW/DSPHLE/`
- **LLE (Low-Level Emulation)** — Full cycle-level DSP processor emulation with its own interpreter and JIT (`Source/Core/Core/DSP/`). Requires DSP ROM dumps. More accurate

The DSP has its own memory map (IRAM 0x2000 bytes, IROM 0x2000 bytes, DRAM 0x2000 bytes, COEF 0x1000 bytes) and register set (4 address registers, 4 index registers, 4 accumulators, product register, status register).

---

## Memory System

### Physical Memory (`HW/Memmap.h`)

| Region | GameCube | Wii | Size |
|--------|----------|-----|------|
| MEM1 (main RAM) | `0x80000000` | `0x80000000` | 24 MB |
| MEM2 (EXRAM) | — | `0x90000000` | 64 MB |
| ARAM (auxiliary) | Separate DMA | Separate DMA | 16 MB |

### Address Translation (`PowerPC/MMU.h`)

Effective addresses are translated through a three-level hierarchy:

1. **BAT (Block Address Translation)** — Fast, covers large (128 KB+) regions. 8 IBAT + 8 DBAT on GameCube; doubled on Wii
2. **TLB (Translation Lookaside Buffer)** — 128-entry cache of page table results, 2-way associative, 4 KB page granularity
3. **Page Table** — Software-walked page table (SDR1 register) for misses

Access API:
- `HostRead/Write<T>()` — Debugger/UI access (safe, no side effects)
- `Read_U8/16/32/64()` / `Write_U8/16/32/64()` — Instruction interpreter access
- `JitCache_TranslateAddress()` — JIT-safe translation with caching

### MMIO (`HW/MMIO.h`)

Memory-mapped I/O registers are located at:
- **GameCube:** `0x0C000000-0x0C008000` (GPU, memory interface, DSP, DVD, SI, EXI, AI)
- **Wii:** additional range at `0x0D000000-0x0D800000` (IPC, USB)

Type-safe handlers registered per address enable read/write dispatch to device emulation.

---

## Hardware Emulation

**Directory:** `Source/Core/Core/HW/`

All hardware is initialized/shutdown through `HW::Init()` / `HW::Shutdown()` in `HW.h`.

### Processor Interface (`ProcessorInterface.h`) — Interrupt Controller

Routes hardware interrupts to the CPU via `PI_INTERRUPT_CAUSE` / `PI_INTERRUPT_MASK` registers:

| Interrupt | Source |
|-----------|--------|
| `INT_CAUSE_PI` | GPU runtime error |
| `INT_CAUSE_DI` | DVD interface |
| `INT_CAUSE_SI` | Serial interface (controllers) |
| `INT_CAUSE_EXI` | Expansion interface |
| `INT_CAUSE_AI` | Audio interface |
| `INT_CAUSE_DSP` | DSP |
| `INT_CAUSE_VI` | Video interface (vsync) |
| `INT_CAUSE_PE_TOKEN/FINISH` | Pixel engine |
| `INT_CAUSE_CP` | Command processor FIFO |
| `INT_CAUSE_WII_IPC` | Wii IPC (Wii only) |

### CoreTiming (`CoreTiming.h`) — Event Scheduler

A cycle-accurate event queue drives all hardware timing. Subsystems register event types and schedule callbacks at specific cycle counts. The CPU checks for pending events via a downcount register after each block.

### Serial Interface (`HW/SI/`) — Controllers

4 SI channels, each supporting:
- GC Controller, Dance Mat, Steering Wheel, Keyboard
- GBA (Game Boy Advance link cable)
- AM Baseboard (arcade)

### Expansion Interface (`HW/EXI/`) — Peripherals

3 EXI channels with chip-select and frequency control:
- Memory cards, IPL ROM, Ethernet adapter, Microphone, Modem

### DVD Interface (`HW/DVD/`) — Disc Drive

Emulates disc reads with DMA transfers and timing simulation. Interfaces with `DiscIO` for actual data access.

### Video Interface (`HW/VideoInterface.h`) — Display Timing

Generates field/frame interrupts, configures video mode (NTSC/PAL/progressive), synchronizes CPU timing with display refresh.

### Audio Interface (`HW/AudioInterface.h`) — Audio Streaming

Controls audio DMA from memory to the audio output, 48 kHz sample rate, stream volume.

### Hardware Initialization Order (`HW::Init()`)

```
CoreTiming → State → AudioInterface → VideoInterface →
SerialInterface → ProcessorInterface → ExpansionInterface →
Memory → AddressSpace → MemoryInterface → DSP →
DVDInterface → GPFifo → CPU → SystemTimers →
[If Wii: WiiIPC → IOS::HLE]
```

---

## GPU / Video Pipeline

### Overview

```
CPU writes to FIFO buffer
        |
CommandProcessor --- parses GPU commands from FIFO
        |
OpcodeDecoder --- interprets GX commands (draw, state change)
        |
VertexLoaderManager --- JIT-compiled vertex format converters
        |
VertexManagerBase --- batches vertices, uploads uniforms
        |
ShaderCache --- generates/caches shaders from GX state
        |
AbstractGfx --- issues draw calls to active backend
        |
Backend (OGL / Vulkan / D3D12 / Metal / Software)
        |
FramebufferManager --- EFB management, resolve, format conversion
        |
Presenter --- XFB presentation to display
```

### VideoCommon (`Source/Core/VideoCommon/`)

Backend-agnostic layer that emulates the GameCube/Wii GPU (nicknamed "Flipper" / "Hollywood"):

| Component | File | Purpose |
|-----------|------|---------|
| **CommandProcessor** | `CommandProcessor.h` | FIFO read/write, breakpoints, watermarks |
| **OpcodeDecoder** | `OpcodeDecoding.cpp` | Decodes GX commands |
| **VertexLoaderManager** | `VertexLoaderManager.h` | Caches JIT-compiled vertex loaders per VCD/VAT combo |
| **VertexManagerBase** | `VertexManagerBase.h` | Vertex batching (48 MB vertex, 8 MB index, 64 MB uniform, 16 MB texel buffers) |
| **ShaderCache** | `ShaderCache.h` | Generates and caches VS/PS/GS from GX state; uber shaders as fallback |
| **PixelShaderGen** | `PixelShaderGen.h` | Pixel shader generation from TEV/alpha/fog/blend state |
| **VertexShaderGen** | `VertexShaderGen.h` | Vertex shader generation from XF/texgen/lighting state |
| **TextureCacheBase** | `TextureCacheBase.h` | Decodes and caches GX textures; supports HiRes replacements |
| **FramebufferManager** | `FramebufferManager.h` | EFB (640x528 native) and XFB (720x576 max) management |
| **BPMemory** | `BPMemory.h` | Backend Processor registers (TEV, blending, EFB, textures) |
| **XFMemory** | `XFMemory.h` | Transform unit registers (projection, texgen, lighting, fog) |
| **CPMemory** | `CPMemory.h` | Command Processor registers (vertex arrays, VCD/VAT) |

### Graphics Abstraction Layer

Every backend implements these abstract interfaces:

| Interface | Purpose |
|-----------|---------|
| `AbstractGfx` | Root API: create textures, shaders, pipelines; issue draws |
| `AbstractTexture` | GPU texture wrapper with format/copy/load operations |
| `AbstractFramebuffer` | Render target (color + depth attachments) |
| `AbstractPipeline` | Complete pipeline state (shaders, blend, depth, raster) |
| `AbstractShader` | Compiled shader object |
| `AbstractStagingTexture` | CPU-GPU transfer buffer (readback, upload, mutable) |
| `NativeVertexFormat` | Backend-specific vertex layout |

Pipeline usage types:
- **GX** — Standard GameCube rendering (per-stage UBO, 8 samplers)
- **GXUber** — Uber shaders with dynamic vertex loading via SSBO
- **Utility** — Post-processing, EFB copies, UI overlays

### Video Backends (`Source/Core/VideoBackends/`)

| Backend | Directory | Key Class | Notes |
|---------|-----------|-----------|-------|
| OpenGL | `OGL/` | `OGLGfx` | GL 4.x+, GLSL, sampler cache |
| Vulkan | `Vulkan/` | `VKGfx` | StateTracker for lazy binding, descriptor sets |
| D3D 11 | `D3D/` | `DXGfx` | Windows only |
| D3D 12 | `D3D12/` | `DX12::Gfx` | Dirty-state tracking, descriptor heap management |
| Metal | `Metal/` | `Metal::Gfx` | macOS/iOS |
| Software | `Software/` | `SWGfx` | CPU rasterizer, accuracy reference |
| Null | `Null/` | `NullGfx` | No-op, for testing/benchmarking |

---

## Audio Subsystem

**Directory:** `Source/Core/AudioCommon/`

### Architecture

```
DSP (HLE or LLE)
      |
   Mixer (AudioCommon/Mixer.h)
   Mixes: DMA audio + streaming audio + Wiimote speaker
          + Skylander portal + GBA link (x4)
      |
   SoundStream (AudioCommon/SoundStream.h)
   Abstract output interface
      |
   Backend: Cubeb / ALSA / PulseAudio / OpenAL / OpenSLES / WASAPI / Null
```

### Audio Backends

| Backend | File | Platform |
|---------|------|----------|
| Cubeb | `CubebStream.cpp` | Cross-platform (primary) |
| ALSA | `AlsaSoundStream.cpp` | Linux |
| PulseAudio | `PulseAudioStream.cpp` | Linux |
| OpenAL | `OpenALStream.cpp` | Cross-platform |
| OpenSLES | `OpenSLESStream.cpp` | Android |
| WASAPI | `WASAPIStream.cpp` | Windows |
| Null | `NullSoundStream.cpp` | Testing |

### Mixer

The `Mixer` manages multiple audio sources through `MixerFifo` instances:
- **DMA audio** (32 kHz) — main game audio from DSP
- **Streaming audio** (48 kHz) — DVD streaming, intro sequences
- **Wiimote speaker** (3 kHz) — Wii Remote speaker audio
- **Skylander portal** (8 kHz) — NFC portal device
- **GBA audio** (48 kHz x4) — Game Boy Advance link cable

Each source uses a circular granule queue (256 granules, 256 samples each) with sample-rate conversion via fixed-point indexing. Optional Dolby Pro Logic II surround decoding via `SurroundDecoder`.

---

## Input Subsystem

**Directory:** `Source/Core/InputCommon/`

### Layer Architecture

```
Physical Devices (keyboard, gamepad, Wiimote, touchscreen)
        |
ControllerInterface (InputCommon/ControllerInterface/)
   Platform backends: SDL, evdev, DInput, XInput, Quartz, etc.
        |
ControllerEmu (InputCommon/ControllerEmu/)
   Emulated controller abstraction (GC pad, Wiimote, Nunchuk, etc.)
        |
ControlReference (InputCommon/ControlReference/)
   Expression-based input mapping ("Button A = `XInput/0/A`")
        |
InputConfig (InputCommon/InputConfig.h)
   Profile save/load, per-game overrides
```

### Controller Interface Backends

| Backend | Directory | Platform |
|---------|-----------|----------|
| SDL | `SDL/` | Cross-platform (primary) |
| evdev | `evdev/` | Linux |
| DInput | `DInput/` | Windows |
| XInput | `XInput/` | Windows (Xbox controllers) |
| WGInput | `WGInput/` | Windows Gaming Input |
| Quartz | `Quartz/` | macOS |
| Xlib | `Xlib/` | Linux (keyboard) |
| Android | `Android/` | Android |
| Wiimote | `Wiimote/` | Native Wiimote via Bluetooth |
| Pipes | `Pipes/` | Named pipes (TAS bots) |
| DualShockUDPClient | `DualShockUDPClient/` | Cemuhook UDP protocol |
| SteamDeck | `SteamDeck/` | Steam Deck haptics |
| Touch | `Touch/` | Touchscreen |

### GC Adapter Support

`GCAdapter.h/cpp` provides native USB support for the official GameCube controller adapter with per-port device detection and rumble.

### Input Flow

1. `ControllerInterface::UpdateInput()` polls all backends each frame
2. Expression parser evaluates bindings (e.g., `XInput/0/A | SDL/0/Button 0`)
3. `ControllerEmu` groups process mapped values (deadzone, calibration, scaling)
4. Final `GCPadStatus` or Wiimote state written to emulated hardware registers

---

## Disc I/O

**Directory:** `Source/Core/DiscIO/`

### Blob Layer (Raw Image Access)

All disc formats are accessed through the abstract `BlobReader` interface (`Blob.h`):

| Format | File | Extension |
|--------|------|-----------|
| Plain ISO | `FileBlob.cpp` | `.iso`, `.gcm` |
| Compressed ISO | `CompressedBlob.cpp` | `.gcz` |
| CISO | `CISOBlob.cpp` | `.ciso` |
| WBFS | `WbfsBlob.cpp` | `.wbfs` |
| WIA/RVZ | `WIABlob.cpp` | `.wia`, `.rvz` |
| NFS | `NFSBlob.cpp` | `.nfs` |
| TGC | `TGCBlob.cpp` | `.tgc` |
| Split | `SplitFileBlob.cpp` | Multi-part |
| Scrubbed | `ScrubbedBlob.cpp` | Junk-data-removed |
| Directory | `DirectoryBlob.cpp` | Virtual disc from folder |

### Volume Layer (Filesystem Access)

`Volume` (`Volume.h`) represents a disc image and provides metadata + partition access:

- `VolumeGC` — GameCube disc (BNR1/BNR2 banners)
- `VolumeWii` — Wii disc (AES-128 partition encryption, SHA1 hash tree verification)
- `VolumeWad` — Wii WAD channel files (ticket, TMD, certificate chain)

### Filesystem Layer

`FileSystemGCWii` (`FileSystemGCWii.h`) reads the FST (file system table) from GC/Wii discs, providing file listing and extraction.

### Additional Features

- `DiscScrubber` — removes unused data for smaller backups
- `VolumeVerifier` — validates disc integrity
- `RiivolutionPatcher` — applies Riivolution mod XML patches
- `NANDImporter` — imports Wii NAND backups
- `DirectoryBlob` — creates virtual disc images from extracted directories

---

## Wii-Specific: IOS Emulation

**Directory:** `Source/Core/Core/IOS/`

The Wii runs a separate ARM-based processor ("Starlet") with its own OS called IOS. Dolphin emulates IOS at the HLE level.

### Architecture

```
Game (PowerPC) <--> IPC messages <--> IOS Kernel (EmulationKernel)
                                            |
                                       IOS Devices
                                       |-- /dev/fs        -- NAND filesystem
                                       |-- /dev/es        -- Title/ticket management
                                       |-- /dev/di        -- Disc interface
                                       |-- /dev/net/*     -- Network stack (sockets, SSL)
                                       |-- /dev/usb/*     -- USB devices
                                       |-- /dev/sdio      -- SD card
                                       |-- /dev/stm       -- State transition manager
                                       '-- /dev/dolphin   -- Dolphin-specific extensions
```

### Key Components

| Component | Directory | Purpose |
|-----------|-----------|---------|
| IOS Kernel | `IOS.h/cpp` | IPC message routing, device management |
| IOSC | `IOSC.h/cpp` | Cryptographic operations (AES, SHA, RSA, ECC) |
| ES | `ES/` | Title installation, ticket/TMD management |
| FS | `FS/` | NAND filesystem emulation |
| Network | `Network/` | Socket, SSL, NCD (network config), KD (WiiConnect24) |
| USB | `USB/` | USB host controller, Skylander portal, Infinity Base |
| DI | `DI/` | Disc interface (Wii mode) |
| WFS | `WFS/` | Wii U filesystem (backward compat) |
| MIOS | `MIOS.h/cpp` | GameCube backward compatibility on Wii |

### IPC Protocol

Devices communicate via IPC commands: `Open`, `Close`, `Read`, `Write`, `Seek`, `Ioctl`, `Ioctlv`. Each device implements the `Device` base class (`Device.h`).

---

## Frontend / UI Layer

Dolphin supports three frontend implementations, all sharing the same core engine.

### DolphinQt — Desktop GUI

**Directory:** `Source/Core/DolphinQt/`

Built on Qt 6. Key components:

| Component | File(s) | Purpose |
|-----------|---------|---------|
| Entry point | `Main.cpp` | App init, Qt event loop setup |
| Main window | `MainWindow.h/cpp` | ~7800 lines; orchestrates all UI panels |
| Game list | `GameList/` | Dual view (list/grid), file monitoring, proxy models |
| Render widget | `RenderWidget.h` | Graphics surface, cursor lock, fullscreen |
| Menu bar | `MenuBar.h` | 100+ menu items, state-aware updates |
| Settings | `Config/` | Modular panels: Graphics, Audio, Controllers, Cheats |
| Debugger | `Debugger/` | 12+ widgets: Code, Memory, Registers, Watch, JIT, Assembler |
| NetPlay | `NetPlay/` | Game hosting/joining, chat, pad mapping |
| Host bridge | `Host.h/cpp` | Singleton mediator between Core and Qt (signals/slots) |

Core-to-UI communication uses `Host` as a bridge: core events become Qt signals via `QueueOnObject`, ensuring thread safety. The Qt event loop integrates with `Core::HostDispatchJobs` via `QAbstractEventDispatcher::aboutToBlock`.

### DolphinNoGUI — Headless/CLI

**Directory:** `Source/Core/DolphinNoGUI/`

Minimal frontend for testing and headless operation. Uses a `Platform` abstraction with implementations for:
- Headless (no window)
- X11, Win32, macOS Cocoa, Linux framebuffer

### Android App

**Directory:** `Source/Android/`

Kotlin/Java application using MVP pattern:

| Component | File | Purpose |
|-----------|------|---------|
| Main activity | `MainActivity.kt` | Game browser with platform tabs |
| Emulation | `EmulationActivity.kt` | Manages emulation session |
| Input overlay | `InputOverlay.kt` | On-screen touch controls |
| Settings | `features/settings/` | Native config wrapper |
| JNI bridge | `NativeLibrary.kt` + `jni/MainAndroid.cpp` | C++ to Java interface |

---

## Networking and NetPlay

### NetPlay Architecture

**Files:** `Source/Core/Core/NetPlayClient.h`, `Source/Core/Core/NetPlayServer.h`, `Source/Core/Core/NetPlayProto.h`

NetPlay enables synchronized multiplayer over the network using a client-server model:

- **Server** (`NetPlayServer`) — Hosts the session, manages pad data distribution, synchronizes game start, verifies game integrity via `SyncIdentifier`
- **Client** (`NetPlayClient`) — Connects to server, sends local input, receives remote input, handles save/code sync
- **Protocol** (`NetPlayProto.h`) — Defines 40+ message types covering connection, input, synchronization, chunked data transfer, golf mode, and game verification
- **Traversal** (`Common/TraversalClient.h`) — NAT traversal via a central server for peer-to-peer connections
- **UPnP** (`Common/UPnP.h`) — Automatic port forwarding

The protocol uses ENet for reliable UDP communication (MTU 1392 bytes, 30-second timeout). Large files (saves, GBA ROMs) are transferred in 16 KB chunks via a dedicated data channel.

### Input Synchronization

- Per-player pad buffers (SPSC queues) for GC controllers and Wiimotes
- Supports "host input authority" mode for reduced latency
- Automatic speed adjustment on buffer underflow
- Golf mode for turn-based controller passing

---

## Common Utilities

**Directory:** `Source/Core/Common/`

### Core Utilities

| Category | Key Files | Purpose |
|----------|-----------|---------|
| **Config** | `Config/Config.h`, `ConfigInfo.h`, `Layer.h` | Layered config system with type-safe cached reads |
| **Logging** | `Logging/Log.h`, `LogManager.h` | 70+ log channels, per-module levels, fmt integration |
| **File I/O** | `FileUtil.h`, `IOFile.h`, `DirectIOFile.h` | Cross-platform file operations, 40+ path indices |
| **Crypto** | `Crypto/AES.h`, `SHA1.h`, `ec.h`, `HMAC.h` | AES-128, SHA1, ECC for Wii encryption |
| **Threading** | `Event.h`, `Flag.h`, `BlockingLoop.h`, `WorkQueueThread.h`, `SPSCQueue.h`, `Mutex.h` | Sync primitives, lock-free queues, spin mutexes |
| **Math** | `MathUtil.h`, `Matrix.h` | Math utilities, matrix operations |
| **Code emitters** | `x64Emitter.h`, `Arm64Emitter.h`, `x64ABI.h` | JIT code generation for x86-64 and ARM64 |
| **String** | `StringUtil.h` | String conversion, UTF-8 helpers, formatting |
| **Network** | `Network.h`, `HttpRequest.h`, `ENet.h`, `TraversalClient.h` | Packet structures, HTTP client, NAT traversal |
| **Serialization** | `ChunkFile.h` | Save state serialization via `PointerWrap` |
| **IniFile** | `IniFile.h` | INI file reader/writer |
| **Hashing** | `Hash.h` | CRC32, xxHash, texture hashing |
| **Assembler** | `Assembler/GekkoAssembler.h` | PowerPC assembler (lexer, parser, IR, codegen) |
| **Bit utils** | `BitField.h`, `BitSet.h`, `BitUtils.h` | Endianness-independent bitfields, bit manipulation |
| **Memory** | `MemoryUtil.h`, `MemArena.h` | Executable memory allocation, W^X enforcement, memory arenas |
| **Debug** | `Debug/Watches.h`, `HostDisassembler.h`, `GekkoDisassembler.h` | Memory watches, x86/ARM/PPC disassembly |

### Configuration System (`Common/Config/`)

Layered architecture with priority resolution:

1. **Base layer** — Hardcoded defaults + user's `Dolphin.ini`
2. **Local layer** — Game-specific INI overrides
3. **CurrentRun layer** — Transient per-session changes

Type-safe access via `Config::Info<T>` descriptors with automatic caching (version-tracked, SpinMutex-protected). Config changes trigger callbacks registered via `Config::AddConfigChangedCallback()`.

---

## Build System

**Primary:** CMake (`CMakeLists.txt` at root, `CMake/` modules)

- **Platforms:** Windows, Linux, macOS, Android (via Gradle + CMake)
- **Visual Studio:** `dolphin-emu.sln` + `.vcxproj` files for Windows development
- **macOS Universal:** `BuildMacOSUniversalBinary.py` for x86_64 + arm64
- **Android:** `Source/Android/` contains Gradle build with CMake integration via JNI
- **Flatpak:** `Flatpak/` for Linux Flatpak packaging

External dependencies in `Externals/` are included as git submodules (50+), including: Qt, Vulkan headers, cubeb, libcurl, SDL, fmt, zstd, lz4, mGBA, rcheevos, imgui, etc.

---

## Key Architectural Patterns

### 1. Singleton System Object
`Core::System::GetInstance()` provides global access to all subsystems. Subsystem managers are lazily created and stored behind a pImpl (`System::Impl`).

### 2. Abstract Factory for Graphics
`AbstractGfx::Create*()` methods return abstract types. Each backend provides concrete implementations, selected at startup based on user preference and platform.

### 3. JIT Compilation Pipeline
PowerPC code -> basic blocks -> register allocation -> native code emission -> block cache. Signal handlers catch memory faults for fastmem translation failures.

### 4. Event-Driven Hardware Timing
`CoreTiming` schedules all hardware events by cycle count. The CPU decrements a counter each block and dispatches events when it reaches zero, avoiding polling.

### 5. FIFO-Based GPU Communication
The CPU writes GX commands to a FIFO buffer. In dual-core mode, the GPU thread reads from this FIFO asynchronously. In single-core mode, the GPU processes commands synchronously.

### 6. Layered Configuration
Config values flow through multiple layers (base -> local -> current run), with game-specific overrides taking highest priority.

### 7. Host Interface Pattern
Frontends implement a `Host` interface that bridges the emulation core to the UI. This decouples the engine from any specific GUI framework.

### 8. State Serialization
All subsystems implement `DoState(PointerWrap&)` for save state support. `PointerWrap` (in `ChunkFile.h`) handles bidirectional serialization.

### 9. HLE/LLE Duality
Both the DSP and the Wii IOS kernel support high-level and low-level emulation approaches, trading accuracy for performance.

### 10. Threading Model

| Thread | Purpose |
|--------|---------|
| **CPU thread** | Executes PowerPC instructions via JIT/interpreter |
| **GPU thread** | Processes FIFO commands (dual-core mode only) |
| **Host/UI thread** | Qt event loop, settings, game list |
| **Audio thread** | Backend-driven audio output, pulls from Mixer |
| **DSP thread** | LLE DSP execution (when using LLE) |

---

## Data Flow Diagrams

### Main Execution Loop

```
CPU::Run()
  '-> PowerPC::RunLoop()
       '-> JIT/Interpreter executes instructions
            |-> Memory access -> MMU translation
            |    |-> BAT lookup (fast)
            |    |-> TLB lookup (cached)
            |    '-> Page table walk (slow)
            |         |-> MMIO? -> Device register handler
            |         '-> RAM? -> Direct memory access
            |-> FIFO write -> GPU command queue
            '-> Downcount expires -> CoreTiming event dispatch
                 |-> VI: vsync interrupt -> frame boundary
                 |-> SI: controller poll
                 |-> AI: audio DMA complete
                 |-> DSP: audio processing
                 |-> EXI: device transfer complete
                 '-> DVD: read complete
```

### Boot Sequence

```
Core::Init() -> spawn EmuThread
  |-> BootManager::SetupConfig() -- per-game settings
  |-> AudioCommon::InitSoundStream()
  |-> Video backend initialization
  |-> HW::Init() -- all hardware in order
  '-> CBoot::BootUp()
       |-> Detect boot type (disc, ELF/DOL, NAND title, WAD)
       |-> SetupMSR() / SetupBAT() -- CPU + address translation
       |-> SetupGCMemory() / SetupWiiMemory() -- RAM init
       |-> RunApploader() or EmulatedBS2_GC/Wii() -- bootstrap
       '-> CPU::Run() -- main loop begins
```

### GPU Rendering Pipeline

```
GX Command (from FIFO)
  |-> State change: Update BP/CP/XF registers
  '-> Draw primitive:
       |-> VertexLoader: Decode vertex format (JIT-compiled)
       |-> VertexManager: Batch into GPU buffers
       '-> On flush:
            |-> Generate/lookup shaders (VS, PS, GS)
            |-> Create/lookup pipeline (shader + render state)
            |-> Upload uniforms (matrices, lighting, fog)
            |-> Bind textures (decoded from GX format, cached)
            '-> Issue draw call -> Backend
                 '-> FramebufferManager resolves and copies to XFB
```

---

## File Quick-Reference

### Entry Points

| Target | File |
|--------|------|
| Qt Desktop | `Source/Core/DolphinQt/Main.cpp` |
| NoGUI / CLI | `Source/Core/DolphinNoGUI/MainNoGUI.cpp` |
| Android | `Source/Android/app/.../MainActivity.kt` |
| CLI Tool | `Source/Core/DolphinTool/` |
| Emulation thread | `Source/Core/Core/Core.cpp` (`EmuThread()`) |

### Core Emulation

| Subsystem | Key Header |
|-----------|------------|
| System root | `Source/Core/Core/System.h` |
| Core API | `Source/Core/Core/Core.h` |
| CPU state | `Source/Core/Core/PowerPC/PowerPC.h` |
| CPU thread | `Source/Core/Core/HW/CPU.h` |
| JIT (x86) | `Source/Core/Core/PowerPC/Jit64/Jit.h` |
| JIT (ARM) | `Source/Core/Core/PowerPC/JitArm64/Jit.h` |
| Interpreter | `Source/Core/Core/PowerPC/Interpreter/Interpreter.h` |
| MMU | `Source/Core/Core/PowerPC/MMU.h` |
| Memory map | `Source/Core/Core/HW/Memmap.h` |
| Interrupts | `Source/Core/Core/HW/ProcessorInterface.h` |
| Event timing | `Source/Core/Core/CoreTiming.h` |
| Boot | `Source/Core/Core/Boot/Boot.h` |
| HW init | `Source/Core/Core/HW/HW.h` |
| IOS kernel | `Source/Core/Core/IOS/IOS.h` |
| HLE hooks | `Source/Core/Core/HLE/HLE.h` |
| Save states | `Source/Core/Core/State.h` |

### Video

| Subsystem | Key Header |
|-----------|------------|
| Graphics API | `Source/Core/VideoCommon/AbstractGfx.h` |
| Textures | `Source/Core/VideoCommon/AbstractTexture.h` |
| Pipelines | `Source/Core/VideoCommon/AbstractPipeline.h` |
| Framebuffers | `Source/Core/VideoCommon/FramebufferManager.h` |
| Vertex batching | `Source/Core/VideoCommon/VertexManagerBase.h` |
| Shader gen (PS) | `Source/Core/VideoCommon/PixelShaderGen.h` |
| Shader gen (VS) | `Source/Core/VideoCommon/VertexShaderGen.h` |
| Shader cache | `Source/Core/VideoCommon/ShaderCache.h` |
| Texture cache | `Source/Core/VideoCommon/TextureCacheBase.h` |
| GX state (BP) | `Source/Core/VideoCommon/BPMemory.h` |
| GX state (XF) | `Source/Core/VideoCommon/XFMemory.h` |
| GX state (CP) | `Source/Core/VideoCommon/CPMemory.h` |
| FIFO | `Source/Core/VideoCommon/Fifo.h` |
| Backend base | `Source/Core/VideoCommon/VideoBackendBase.h` |

### Hardware Devices

| Device | Key Header |
|--------|------------|
| SI (controllers) | `Source/Core/Core/HW/SI/SI.h` |
| EXI (memory card) | `Source/Core/Core/HW/EXI/EXI.h` |
| DVD drive | `Source/Core/Core/HW/DVD/DVDInterface.h` |
| DSP | `Source/Core/Core/HW/DSP.h` |
| DSP HLE | `Source/Core/Core/HW/DSPHLE/` |
| DSP LLE | `Source/Core/Core/HW/DSPLLE/` |
| DSP Core | `Source/Core/Core/DSP/DSPCore.h` |
| Video interface | `Source/Core/Core/HW/VideoInterface.h` |
| Audio interface | `Source/Core/Core/HW/AudioInterface.h` |

### I/O and Input

| Subsystem | Key Header |
|-----------|------------|
| Disc blobs | `Source/Core/DiscIO/Blob.h` |
| Disc volumes | `Source/Core/DiscIO/Volume.h` |
| GC/Wii FS | `Source/Core/DiscIO/FileSystemGCWii.h` |
| Controller interface | `Source/Core/InputCommon/ControllerInterface/ControllerInterface.h` |
| Controller emulation | `Source/Core/InputCommon/ControllerEmu/` |
| Input config | `Source/Core/InputCommon/InputConfig.h` |
| GC adapter | `Source/Core/InputCommon/GCAdapter.h` |
| Audio mixer | `Source/Core/AudioCommon/Mixer.h` |
| Sound output | `Source/Core/AudioCommon/SoundStream.h` |

### Common

| Utility | Key Header |
|---------|------------|
| Config system | `Source/Core/Common/Config/Config.h` |
| Logging | `Source/Core/Common/Logging/Log.h` |
| File utilities | `Source/Core/Common/FileUtil.h` |
| Save states | `Source/Core/Common/ChunkFile.h` |
| x86 code gen | `Source/Core/Common/x64Emitter.h` |
| ARM code gen | `Source/Core/Common/Arm64Emitter.h` |
| INI files | `Source/Core/Common/IniFile.h` |
| Thread utils | `Source/Core/Common/Event.h`, `Flag.h`, `BlockingLoop.h` |
| Bitfields | `Source/Core/Common/BitField.h` |
| Memory mgmt | `Source/Core/Common/MemoryUtil.h` |
| Network | `Source/Core/Common/Network.h` |
| Crypto | `Source/Core/Common/Crypto/AES.h`, `SHA1.h` |

### NetPlay

| Subsystem | Key Header |
|-----------|------------|
| Client | `Source/Core/Core/NetPlayClient.h` |
| Server | `Source/Core/Core/NetPlayServer.h` |
| Protocol | `Source/Core/Core/NetPlayProto.h` |
| Game ID | `Source/Core/Core/SyncIdentifier.h` |
| Traversal | `Source/Core/Common/TraversalClient.h` |
