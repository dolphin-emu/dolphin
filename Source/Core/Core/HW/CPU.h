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

// Enable or Disable Stepping [Not threadsafe]
void EnableStepping(bool stepping);

// Breakpoint activation. Similar to EnableStepping(true), except it's threadsafe.
void Break();

// Is stepping ?
bool IsStepping();

// Waits until is stepping and is ready for a command (paused and fully idle), and acquires a lock on that state.
// or, if doLock is false, releases a lock on that state and optionally re-disables stepping.
// calls must be balanced and non-recursive (once with doLock true, then once with doLock false).
// e.g. when the GUI thread wants to make sure everything is paused so that it can create a savestate.
// the return value is whether the CPU was unpaused before the call.
bool PauseAndLock(bool do_lock, bool unpause_on_unlock = true);

// This is an accessor function for FifoPlayer to more effectively lie about being the
// CPU. It needs access to the step event in order to handle CPU::EnableStepping(...).
void StealStepEventAndWaitOnIt();

// These are access functions for Core to access the CPU locking scheme.
// The Core has it's own Pause/Unpause logic which is separate but also entirely dependent
// on the CPU locking so we need to share the same locks.
// As implied by the names, these can only be called from the Host (GUI) thread.
// NOTE: These are recursive locks.
void HostEnterEnableSteppingLock();
void HostLeaveEnableSteppingLock(bool did_enable_stepping);
bool HostAllowUnpauseWhenLeavingPauseAndLock();

}
