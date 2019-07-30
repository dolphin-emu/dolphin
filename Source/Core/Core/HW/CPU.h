// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace Common
{
class Event;
}

namespace PowerPC
{
enum class CPUCore;
}

namespace CPU
{
enum class State
{
  Running = 0,
  Stepping = 2,
  PowerDown = 3
};

// Init
void Init(PowerPC::CPUCore cpu_core);

// Shutdown
void Shutdown();

// Starts the CPU
// To be called by the CPU Thread.
void Run();

// Causes shutdown
// Once started, State::PowerDown cannot be stopped.
// Synchronizes with the CPU Thread (waits for CPU::Run to exit).
void Stop();

// Reset [NOT IMPLEMENTED]
void Reset();

// StepOpcode (Steps one Opcode)
void StepOpcode(Common::Event* event = nullptr);

// Enable or Disable Stepping. [Will deadlock if called from a system thread]
void EnableStepping(bool stepping);

// Breakpoint activation for system threads. Similar to EnableStepping(true).
// NOTE: Unlike EnableStepping, this does NOT synchronize with the CPU Thread
//   which enables it to avoid deadlocks but also makes it less safe so it
//   should not be used by the Host.
void Break();

// Shorthand for GetState() == State::Stepping.
// WARNING: State::PowerDown will return false, not just State::Running.
bool IsStepping();

// Get current CPU Thread State
State GetState();

// Direct State Access (Raw pointer for embedding into JIT Blocks)
// Strictly read-only. A lock is required to change the value.
const State* GetStatePtr();

// Locks the CPU Thread (waiting for it to become idle).
// While this lock is held, the CPU Thread will not perform any action so it is safe to access
// PowerPC::ppcState, CoreTiming, etc. without racing the CPU Thread.
// Cannot be used recursively. Must be paired as PauseAndLock(true)/PauseAndLock(false).
// Return value for do_lock == true is whether the state was State::Running or not.
// Return value for do_lock == false is whether the state was changed *to* State::Running or not.
// Cannot be used by System threads as it will deadlock. It is threadsafe otherwise.
// "control_adjacent" causes PauseAndLock to behave like EnableStepping by modifying the
//   state of the Audio and FIFO subsystems as well.
bool PauseAndLock(bool do_lock, bool unpause_on_unlock = true, bool control_adjacent = false);
}  // namespace CPU
