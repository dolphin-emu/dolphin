# Type stub for the embedded `dolphin.event` module.
# Two ways to react to emulator events:
#   1. register a callback with on_*(callback); pass None to unregister.
#   2. await the same-named coroutine inside an async script.
# Emulation events fire on the emulation thread, synchronous to the frame.
# hostupdate fires off a UI-thread timer (~60Hz) even while emulation is paused.
# WARNING: hostupdate runs on the host thread, NOT the emu thread. Do not read emulated
# memory/registers from it (races the CPU core, crashes / "unable to resolve read address").
# Cache state in a frameadvance handler; have hostupdate touch only cached values and UI state.

from typing import Awaitable, Callable, Optional, Tuple

# --- callback registration ---

def on_frameadvance(callback: Optional[Callable[[], object]]) -> None: ...
def on_hostupdate(callback: Optional[Callable[[], object]]) -> None: ...  # ~60Hz, fires while paused
def on_framedrawn(callback: Optional[Callable[[int, int, bytes], object]]) -> None: ...  # width, height, RGBA bytes
def on_memorybreakpoint(callback: Optional[Callable[[bool, int, int], object]]) -> None: ...  # is_write, addr, value
def on_codebreakpoint(callback: Optional[Callable[[int], object]]) -> None: ...  # addr
def on_savestatesave(callback: Optional[Callable[[bool, int], object]]) -> None: ...  # to_slot, slot
def on_savestateload(callback: Optional[Callable[[bool, int], object]]) -> None: ...  # from_slot, slot

# --- awaitable coroutines (resolve to the same args the callbacks receive) ---

def frameadvance() -> Awaitable[None]: ...
def hostupdate() -> Awaitable[None]: ...
def framedrawn() -> Awaitable[Tuple[int, int, bytes]]: ...
def memorybreakpoint() -> Awaitable[Tuple[bool, int, int]]: ...
def codebreakpoint() -> Awaitable[Tuple[int]]: ...
def savestatesave() -> Awaitable[Tuple[bool, int]]: ...
def savestateload() -> Awaitable[Tuple[bool, int]]: ...

def system_reset() -> None: ...                 # tap the console reset button
