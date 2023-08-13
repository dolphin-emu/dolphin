# Slippi Rust EXI Device
This module acts as a "shadow" EXI device for the _actual_ C++ EXI Device ([`EXI_DeviceSlippi.cpp`](../../../Source/Core/Core/HW/EXI_DeviceSlippi.cpp)) over on the Dolphin side of things. The C++ class _owns_ this EXI device and acts as the general entry point to everything.

If you're building something on the Rust side, you probably - unless it's a standalone function or you really know it needs to be global - want to just hook it here. Doing so will ensure you get the automatic lifecycle management and can avoid worrying about any Rust cleanup pieces (just implement `Drop`).

For an example of this pattern, check out how the `Jukebox` is orchestrated in this crate.
