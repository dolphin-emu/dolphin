// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/CPU.h"

#include <condition_variable>
#include <mutex>

#include "AudioCommon/AudioCommon.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/PowerPC/PowerPC.h"
#include "VideoCommon/Fifo.h"

namespace CPU
{
// CPU Thread execution state.
// Requires s_state_change_lock to modify the value.
// Read access is unsynchronized.
static State s_state = State::PowerDown;

// Synchronizes EnableStepping and PauseAndLock so only one instance can be
// active at a time. Simplifies code by eliminating several edge cases where
// the EnableStepping(true)/PauseAndLock(true) case must release the state lock
// and wait for the CPU Thread which would otherwise require additional flags.
// NOTE: When using the stepping lock, it must always be acquired first. If
//   the lock is acquired after the state lock then that is guaranteed to
//   deadlock because of the order inversion. (A -> X,Y; B -> Y,X; A waits for
//   B, B waits for A)
static std::mutex s_stepping_lock;

// Primary lock. Protects changing s_state, requesting instruction stepping and
// pause-and-locking.
static std::mutex s_state_change_lock;
// When s_state_cpu_thread_active changes to false
static std::condition_variable s_state_cpu_idle_cvar;
// When s_state changes / s_state_paused_and_locked becomes false (for CPU Thread only)
static std::condition_variable s_state_cpu_cvar;
static bool s_state_cpu_thread_active = false;
static bool s_state_paused_and_locked = false;
static bool s_state_system_request_stepping = false;
static bool s_state_cpu_step_instruction = false;
static Common::Event* s_state_cpu_step_instruction_sync = nullptr;

void Init(int cpu_core)
{
  PowerPC::Init(cpu_core);
  s_state = State::Stepping;
}

void Shutdown()
{
  Stop();
  PowerPC::Shutdown();
}

// Requires holding s_state_change_lock
static void FlushStepSyncEventLocked()
{
  if (s_state_cpu_step_instruction_sync)
  {
    s_state_cpu_step_instruction_sync->Set();
    s_state_cpu_step_instruction_sync = nullptr;
  }
  s_state_cpu_step_instruction = false;
}

void Run()
{
  std::unique_lock<std::mutex> state_lock(s_state_change_lock);
  while (s_state != State::PowerDown)
  {
    s_state_cpu_cvar.wait(state_lock, [] { return !s_state_paused_and_locked; });

    switch (s_state)
    {
    case State::Running:
      s_state_cpu_thread_active = true;
      state_lock.unlock();

      // Adjust PC for JIT when debugging
      // SingleStep so that the "continue", "step over" and "step out" debugger functions
      // work when the PC is at a breakpoint at the beginning of the block
      // If watchpoints are enabled, any instruction could be a breakpoint.
      if (PowerPC::GetMode() != PowerPC::CoreMode::Interpreter)
      {
        if (PowerPC::breakpoints.IsAddressBreakPoint(PC) || PowerPC::memchecks.HasAny())
        {
          s_state = State::Stepping;
          PowerPC::CoreMode old_mode = PowerPC::GetMode();
          PowerPC::SetMode(PowerPC::CoreMode::Interpreter);
          PowerPC::SingleStep();
          PowerPC::SetMode(old_mode);
          s_state = State::Running;
        }
      }

      // Enter a fast runloop
      PowerPC::RunLoop();

      state_lock.lock();
      s_state_cpu_thread_active = false;
      s_state_cpu_idle_cvar.notify_all();
      break;

    case State::Stepping:
      // Wait for step command.
      s_state_cpu_cvar.wait(state_lock,
                            [] { return s_state_cpu_step_instruction || !IsStepping(); });
      if (!IsStepping())
      {
        // Signal event if the mode changes.
        FlushStepSyncEventLocked();
        continue;
      }
      if (s_state_paused_and_locked)
        continue;

      // Do step
      s_state_cpu_thread_active = true;
      state_lock.unlock();

      PowerPC::SingleStep();

      state_lock.lock();
      s_state_cpu_thread_active = false;
      s_state_cpu_idle_cvar.notify_all();

      // Update disasm dialog
      FlushStepSyncEventLocked();
      Host_UpdateDisasmDialog();
      break;

    case State::PowerDown:
      break;
    }
  }
  state_lock.unlock();
  Host_UpdateDisasmDialog();
}

// Requires holding s_state_change_lock
static void RunAdjacentSystems(bool running)
{
  // NOTE: We're assuming these will not try to call Break or EnableStepping.
  Fifo::EmulatorState(running);
  AudioCommon::ClearAudioBuffer(!running);
}

void Stop()
{
  // Change state and wait for it to be acknowledged.
  // We don't need the stepping lock because State::PowerDown is a priority state which
  // will stick permanently.
  std::unique_lock<std::mutex> state_lock(s_state_change_lock);
  s_state = State::PowerDown;
  s_state_cpu_cvar.notify_one();

  while (s_state_cpu_thread_active)
  {
    std::cv_status status =
        s_state_cpu_idle_cvar.wait_for(state_lock, std::chrono::milliseconds(100));
    if (status == std::cv_status::timeout)
      Host_YieldToUI();
  }

  RunAdjacentSystems(false);
  FlushStepSyncEventLocked();
}

bool IsStepping()
{
  return s_state == State::Stepping;
}

State GetState()
{
  return s_state;
}

const State* GetStatePtr()
{
  return &s_state;
}

void Reset()
{
}

void StepOpcode(Common::Event* event)
{
  std::lock_guard<std::mutex> state_lock(s_state_change_lock);
  // If we're not stepping then this is pointless
  if (!IsStepping())
  {
    if (event)
      event->Set();
    return;
  }

  // Potential race where the previous step has not been serviced yet.
  if (s_state_cpu_step_instruction_sync && s_state_cpu_step_instruction_sync != event)
    s_state_cpu_step_instruction_sync->Set();

  s_state_cpu_step_instruction = true;
  s_state_cpu_step_instruction_sync = event;
  s_state_cpu_cvar.notify_one();
}

// Requires s_state_change_lock
static bool SetStateLocked(State s)
{
  if (s_state == State::PowerDown)
    return false;
  s_state = s;
  return true;
}

void EnableStepping(bool stepping)
{
  std::lock_guard<std::mutex> stepping_lock(s_stepping_lock);
  std::unique_lock<std::mutex> state_lock(s_state_change_lock);

  if (stepping)
  {
    SetStateLocked(State::Stepping);

    while (s_state_cpu_thread_active)
    {
      std::cv_status status =
          s_state_cpu_idle_cvar.wait_for(state_lock, std::chrono::milliseconds(100));
      if (status == std::cv_status::timeout)
        Host_YieldToUI();
    }

    RunAdjacentSystems(false);
  }
  else if (SetStateLocked(State::Running))
  {
    s_state_cpu_cvar.notify_one();
    RunAdjacentSystems(true);
  }
}

void Break()
{
  std::lock_guard<std::mutex> state_lock(s_state_change_lock);

  // If another thread is trying to PauseAndLock then we need to remember this
  // for later to ignore the unpause_on_unlock.
  if (s_state_paused_and_locked)
  {
    s_state_system_request_stepping = true;
    return;
  }

  // We'll deadlock if we synchronize, the CPU may block waiting for our caller to
  // finish resulting in the CPU loop never terminating.
  SetStateLocked(State::Stepping);
  RunAdjacentSystems(false);
}

bool PauseAndLock(bool do_lock, bool unpause_on_unlock, bool control_adjacent)
{
  // NOTE: This is protected by s_stepping_lock.
  static bool s_have_fake_cpu_thread = false;
  bool was_unpaused = false;

  if (do_lock)
  {
    s_stepping_lock.lock();

    std::unique_lock<std::mutex> state_lock(s_state_change_lock);
    s_state_paused_and_locked = true;

    was_unpaused = s_state == State::Running;
    SetStateLocked(State::Stepping);

    while (s_state_cpu_thread_active)
    {
      std::cv_status status =
          s_state_cpu_idle_cvar.wait_for(state_lock, std::chrono::milliseconds(100));
      if (status == std::cv_status::timeout)
        Host_YieldToUI();
    }

    if (control_adjacent)
      RunAdjacentSystems(false);
    state_lock.unlock();

    // NOTE: It would make more sense for Core::DeclareAsCPUThread() to keep a
    //   depth counter instead of being a boolean.
    if (!Core::IsCPUThread())
    {
      s_have_fake_cpu_thread = true;
      Core::DeclareAsCPUThread();
    }
  }
  else
  {
    // Only need the stepping lock for this
    if (s_have_fake_cpu_thread)
    {
      s_have_fake_cpu_thread = false;
      Core::UndeclareAsCPUThread();
    }

    {
      std::lock_guard<std::mutex> state_lock(s_state_change_lock);
      if (s_state_system_request_stepping)
      {
        s_state_system_request_stepping = false;
      }
      else if (unpause_on_unlock && SetStateLocked(State::Running))
      {
        was_unpaused = true;
      }
      s_state_paused_and_locked = false;
      s_state_cpu_cvar.notify_one();

      if (control_adjacent)
        RunAdjacentSystems(s_state == State::Running);
    }
    s_stepping_lock.unlock();
  }
  return was_unpaused;
}
}
