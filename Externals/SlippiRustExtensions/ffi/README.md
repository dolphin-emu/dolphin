# Slippi Rust FFI
This crate contains the external API that Dolphin can see and interact with. The API itself is a small set of C functions that handle creating/destroying/updating various components (EXI, Logger, Jukebox, etc) on the Rust side of things.

The included `build.rs` will automatically generate a C header set on each build, and the CMake and Visual Studio projects are configured to locate it from the `includes` folder.

When adding a new method to this crate, prefix it with `slprs_` so that the Dolphin codebase keeps a clear delineation of where Rust code is being used. If (or when) you use `unsafe`, please add a comment explaining _why_ we can guarantee some aspect of safety.

See the method headers for more information.
