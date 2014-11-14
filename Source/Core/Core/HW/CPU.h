// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace Common {
	class Event;
}

class CCPU
{
public:
	// init
	static void Init(int cpu_core);

	// shutdown
	static void Shutdown();

	// starts the CPU
	static void Run();

	// causes shutdown
	static void Stop();
	// Reset
	static void Reset();

	// StepOpcode (Steps one Opcode)
	static void StepOpcode(Common::Event *event = nullptr);

	// one step only
	static void SingleStep();

	// Enable or Disable Stepping
	static void EnableStepping(const bool _bStepping);

	// break, same as EnableStepping(true).
	static void Break();

	// is stepping ?
	static bool IsStepping();

	// waits until is stepping and is ready for a command (paused and fully idle), and acquires a lock on that state.
	// or, if doLock is false, releases a lock on that state and optionally re-disables stepping.
	// calls must be balanced and non-recursive (once with doLock true, then once with doLock false).
	// intended (but not required) to be called from another thread,
	// e.g. when the GUI thread wants to make sure everything is paused so that it can create a savestate.
	// the return value is whether the CPU was unpaused before the call.
	static bool PauseAndLock(bool doLock, bool unpauseOnUnlock=true);
};
