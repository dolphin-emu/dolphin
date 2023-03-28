// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

namespace Common
{
class Event;
}
namespace Core
{
class System;
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

class CPUManager
{
public:
  explicit CPUManager(Core::System& system);
  CPUManager(const CPUManager& other) = delete;
  CPUManager(CPUManager&& other) = delete;
  CPUManager& operator=(const CPUManager& other) = delete;
  CPUManager& operator=(CPUManager&& other) = delete;
  ~CPUManager();

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

  // This should only be called from the CPU thread
  void Continue();

  // Shorthand for GetState() == State::Stepping.
  // WARNING: State::PowerDown will return false, not just State::Running.
  bool IsStepping() const;

  // Get current CPU Thread State
  State GetState() const;

  // Direct State Access (Raw pointer for embedding into JIT Blocks)
  // Strictly read-only. A lock is required to change the value.
  const State* GetStatePtr() const;

  // Locks the CPU Thread (waiting for it to become idle).
  // While this lock is held, the CPU Thread will not perform any action so it is safe to access
  // PowerPC, CoreTiming, etc. without racing the CPU Thread.
  // Cannot be used recursively. Must be paired as PauseAndLock(true)/PauseAndLock(false).
  // Return value for do_lock == true is whether the state was State::Running or not.
  // Return value for do_lock == false is whether the state was changed *to* State::Running or not.
  // Cannot be used by System threads as it will deadlock. It is threadsafe otherwise.
  // "control_adjacent" causes PauseAndLock to behave like EnableStepping by modifying the
  //   state of the Audio and FIFO subsystems as well.
  bool PauseAndLock(bool do_lock, bool unpause_on_unlock = true, bool control_adjacent = false);

  // Adds a job to be executed during on the CPU thread. This should be combined with
  // PauseAndLock(), as while the CPU is in the run loop, it won't execute the function.
  void AddCPUThreadJob(std::function<void()> function);

private:
  void FlushStepSyncEventLocked();
  void ExecutePendingJobs(std::unique_lock<std::mutex>& state_lock);
  void RunAdjacentSystems(bool running);
  bool SetStateLocked(State s);

  // CPU Thread execution state.
  // Requires m_state_change_lock to modify the value.
  // Read access is unsynchronized.
  State m_state = State::PowerDown;

  // Synchronizes EnableStepping and PauseAndLock so only one instance can be
  // active at a time. Simplifies code by eliminating several edge cases where
  // the EnableStepping(true)/PauseAndLock(true) case must release the state lock
  // and wait for the CPU Thread which would otherwise require additional flags.
  // NOTE: When using the stepping lock, it must always be acquired first. If
  //   the lock is acquired after the state lock then that is guaranteed to
  //   deadlock because of the order inversion. (A -> X,Y; B -> Y,X; A waits for
  //   B, B waits for A)
  std::mutex m_stepping_lock;

  // Primary lock. Protects changing m_state, requesting instruction stepping and
  // pause-and-locking.
  std::mutex m_state_change_lock;
  // When m_state_cpu_thread_active changes to false
  std::condition_variable m_state_cpu_idle_cvar;
  // When m_state changes / m_state_paused_and_locked becomes false (for CPU Thread only)
  std::condition_variable m_state_cpu_cvar;
  bool m_state_cpu_thread_active = false;
  bool m_state_paused_and_locked = false;
  bool m_state_system_request_stepping = false;
  bool m_state_cpu_step_instruction = false;
  Common::Event* m_state_cpu_step_instruction_sync = nullptr;
  std::queue<std::function<void()>> m_pending_jobs;

  Core::System& m_system;
};
}  // namespace CPU
