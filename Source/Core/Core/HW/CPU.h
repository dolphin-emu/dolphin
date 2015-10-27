// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace Common {
	class Event;
}

namespace CPU
{

// Init
void Init(int cpu_core);

// Shutdown
void Shutdown();

// Starts the CPU
void Run();

// Causes shutdown
void Stop();

// Reset
void Reset();

// StepOpcode (Steps one Opcode)
void StepOpcode(Common::Event* event = nullptr);

// Enable or Disable Stepping
void EnableStepping(bool stepping);

// Break, same as EnableStepping(true).
void Break();

// Is stepping ?
bool IsStepping();

// Waits until is stepping and is ready for a command (paused and fully idle), and acquires a lock on that state.
// or, if doLock is false, releases a lock on that state and optionally re-disables stepping.
// calls must be balanced and non-recursive (once with doLock true, then once with doLock false).
// intended (but not required) to be called from another thread,
// e.g. when the GUI thread wants to make sure everything is paused so that it can create a savestate.
// the return value is whether the CPU was unpaused before the call.
bool PauseAndLock(bool do_lock, bool unpause_on_unlock = true);

}
