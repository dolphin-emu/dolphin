# Slippi Rust Extensions
This external module houses various Slippi-specific bits of functionality and is ultimately linked into the Dolphin executable and instrumented via the C FFI. This is an evolving workspace area and may be subject to changes.

This workspace currently targets Rust `1.70.0`. As long as you have Rust installed, `cargo` should automatically configure this when building.

> You may opt to add other components (e.g `clippy`) as the toolchain file currently targets what the CI builders need.

- [Project Module Structure Overview](#project-module-structure-overview)
- [Adding a new workspace module](#adding-a-new-workspace-module)
- [Feature Flags](#feature-flags)
- [Building out of band](#building-out-of-band)

## Project Module Structure Overview

| Module                 | Description                                                                |
|------------------------|----------------------------------------------------------------------------|
| `dolphin-integrations` | A library that wraps Dolphin callbacks (logging, etc).                     |
| `exi`                  | EXI device that receives forwarded calls from the EXI (C++) device.        |
| `ffi`                  | The core library. Exposes C FFI functions for Dolphin to call.             |
| `game-reporter`        | Implements match and event reporting.                                      |
| `jukebox`              | Melee music player library. See the [Slippi Jukebox README](jukebox/README.md) for more info. |

Some important aspects of the project structure to understand:

- The build script in this crate automatically generates C bindings that get output to `ffi/includes/SlippiRustExtensions.h`, and the Dolphin CMake and Visual Studio projects are pre-configured to find this header and link the necessary libraries and dlls.
- The **entry point** of this is the `ffi` crate. (e.g, `Dolphin (C++) -> ffi (C) -> (EXI) (Rust)`)
- As Rust is not the _host_ language, it cannot import anything from the C++ side of things. This means that when you go to bridge things, you'll need to thread calls and data through from the C++ side of things to the Rust side of things. In practice, this shouldn't be too bad - much of the Slippi-specific logic is contained within the `EXI_DeviceSlippi.cpp` class.
- The aforementioned `EXI_DeviceSlippi.cpp` class *owns* the Rust EXI device. When the EXI device is created, the Rust EXI device is created. When the EXI device is torn down, the Rust device is torn down. Do not alter this lifecycle pattern; it keeps things predictable regarding ownership and fits well with the Rust approach.
- If you add a new FFI function, please prefix it with `slprs_`. This helps keep the Dolphin codebase well segmented for "Slippi-Rust"-specific functionality.

> Be careful when trying to pass C++ types! _C_ has a stable ABI, _C++_ does not - you **always go through C**.

## Adding a new workspace module
If you're adding a new workspace module, simply create it (`cargo new --lib my_module`), add it to the root `Cargo.toml`, and setup/link it/call it elsewhere accordingly.

If this is code that Dolphin needs to call (via the C FFI), then it belongs in the `ffi` module. Your exposed method in the `ffi` module can/should forward on to wherever your code actually lives.

## Feature Flags

### The `ishiiruka/mainline` feature(s)
These two features control which codebase this extension is built against: Ishiiruka (the older build) or Dolphin (mainline Dolphin, much newer). `ishiiruka` is the current default feature until further notice.

### The `playback` feature
There is an optional feature flag for this repository for playback-specific functionality. This will automatically be set via either CMake or Visual Studio if you're building a Playback-enabled project, but if you're building and testing out of band you may need to enable this flag, e.g:

```
cargo build --release --features playback
```

## Building out of band

#### Windows
To compile changes made to the Rust extensions without having to rebuild the entire Dolphin project, you can compile _just_ the Rust codebase in the target area. This can work as the Rust code is linked to the Dolphin side of things at runtime; if you alter the Dolphin codebase, _or if you alter the exposed C FFI_, you will need a full project rebuild.

In the `SlippiRustExtensions` directory, run the following:

```
cargo build --release --target=x86_64-pc-windows-msvc
```

> This is necessary on Windows to allow for situations where developers may need to run a VM to test across OS instances. The default `release` path can conflict on the two when mounted on the same filesystem, and we need the Visual Studio builder to know where to reliably look.

#### macOS/Linux/etc
Simply rebuild with the release flag.

```
cargo build --release
```

## Code Formatting
The following line will format the entire project
```
cargo fmt --all --manifest-path=./Cargo.toml
```
