// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/CPU.h"

#include <condition_variable>
#include <mutex>
#include <queue>

#include "AudioCommon/AudioCommon.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/CPUThreadConfigCallback.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/PowerPC/GDBStub.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "Core/TimePlayed.h"
#include "VideoCommon/Fifo.h"

namespace CPU
{
CPUManager::CPUManager(Core::System& system) : m_system(system)
{
}
CPUManager::~CPUManager() = default;

void CPUManager::Init(PowerPC::CPUCore cpu_core)
{
  m_system.GetPowerPC().Init(cpu_core);
  m_state = State::Stepping;
}

void CPUManager::Shutdown()
{
  Stop();
  m_system.GetPowerPC().Shutdown();
}

// Requires holding m_state_change_lock
void CPUManager::FlushStepSyncEventLocked()
{
  if (!m_state_cpu_step_instruction)
    return;

  if (m_state_cpu_step_instruction_sync)
  {
    m_state_cpu_step_instruction_sync->Set();
    m_state_cpu_step_instruction_sync = nullptr;
  }
  m_state_cpu_step_instruction = false;
}

void CPUManager::ExecutePendingJobs(std::unique_lock<std::mutex>& state_lock)
{
  while (!m_pending_jobs.empty())
  {
    auto callback = std::move(m_pending_jobs.front());
    m_pending_jobs.pop();
    state_lock.unlock();
    callback();
    state_lock.lock();
  }
}

void CPUManager::StartTimePlayedTimer()
{
  Common::SetCurrentThreadName("Play Time Tracker");

  // Use a clock that will appropriately ignore suspended system time.
  Common::SteadyAwakeClock timer;
  auto prev_time = timer.now();

  while (true)
  {
    TimePlayed time_played;
    auto curr_time = timer.now();

    // Check that emulation is not paused
    // If the emulation is paused, wait for SetStepping() to reactivate
    if (m_state == State::Running)
    {
      const std::string game_id = SConfig::GetInstance().GetGameID();
      const auto diff_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - prev_time);
      time_played.AddTime(game_id, diff_time);
    }
    else if (m_state == State::Stepping)
    {
      m_time_played_finish_sync.Wait();
      curr_time = timer.now();
    }

    prev_time = curr_time;

    if (m_state == State::PowerDown)
      return;

    m_time_played_finish_sync.WaitFor(std::chrono::seconds(30));
  }
}

void CPUManager::Run()
{
  auto& power_pc = m_system.GetPowerPC();

  // Start a separate time tracker thread
  std::thread timing;
  if (Config::Get(Config::MAIN_TIME_TRACKING))
  {
    timing = std::thread(&CPUManager::StartTimePlayedTimer, this);
  }

  std::unique_lock state_lock(m_state_change_lock);
  while (m_state != State::PowerDown)
  {
    m_state_cpu_cvar.wait(state_lock, [this] { return !m_state_paused_and_locked; });
    ExecutePendingJobs(state_lock);
    CPUThreadConfigCallback::CheckForConfigChanges();

    Common::Event gdb_step_sync_event;
    switch (m_state)
    {
    case State::Running:
      m_state_cpu_thread_active = true;
      state_lock.unlock();

      // Adjust PC when debugging
      // SingleStep so that the "continue", "step over" and "step out" debugger functions
      // work when the PC is at a breakpoint at the beginning of the block
      // Don't use PowerPCManager::CheckBreakPoints, otherwise you get double logging
      // If watchpoints are enabled, any instruction could be a breakpoint.
      if (power_pc.GetBreakPoints().IsAddressBreakPoint(power_pc.GetPPCState().pc) ||
          power_pc.GetMemChecks().HasAny())
      {
        m_state = State::Stepping;
        PowerPC::CoreMode old_mode = power_pc.GetMode();
        power_pc.SetMode(PowerPC::CoreMode::Interpreter);
        power_pc.SingleStep();
        power_pc.SetMode(old_mode);
        m_state = State::Running;
      }

      // Enter a fast runloop
      power_pc.RunLoop();

      state_lock.lock();
      m_state_cpu_thread_active = false;
      m_state_cpu_idle_cvar.notify_all();
      break;

    case State::Stepping:
      // Wait for step command.
      m_state_cpu_cvar.wait(state_lock, [this, &state_lock, &gdb_step_sync_event] {
        ExecutePendingJobs(state_lock);
        CPUThreadConfigCallback::CheckForConfigChanges();
        state_lock.unlock();
        if (GDBStub::IsActive() && GDBStub::HasControl())
        {
          if (!GDBStub::JustConnected())
            GDBStub::SendSignal(GDBStub::Signal::Sigtrap);
          GDBStub::ProcessCommands(true);
          // If we are still going to step, emulate the fact we just sent a step command
          if (GDBStub::HasControl())
          {
            // Make sure the previous step by gdb was serviced
            if (m_state_cpu_step_instruction_sync &&
                m_state_cpu_step_instruction_sync != &gdb_step_sync_event)
            {
              m_state_cpu_step_instruction_sync->Set();
            }

            m_state_cpu_step_instruction = true;
            m_state_cpu_step_instruction_sync = &gdb_step_sync_event;
          }
        }
        state_lock.lock();
        return m_state_cpu_step_instruction || !IsStepping();
      });
      if (!IsStepping())
      {
        // Signal event if the mode changes.
        FlushStepSyncEventLocked();
        continue;
      }
      if (m_state_paused_and_locked)
        continue;

      // Do step
      m_state_cpu_thread_active = true;
      state_lock.unlock();

      power_pc.SingleStep();

      state_lock.lock();
      m_state_cpu_thread_active = false;
      m_state_cpu_idle_cvar.notify_all();

      // Update disasm dialog
      FlushStepSyncEventLocked();
      Host_UpdateDisasmDialog();
      break;

    case State::PowerDown:
      break;
    }
  }

  if (timing.joinable())
  {
    m_time_played_finish_sync.Set();
    timing.join();
  }

  state_lock.unlock();
  Host_UpdateDisasmDialog();
}

// Requires holding m_state_change_lock
void CPUManager::RunAdjacentSystems(bool running)
{
  // NOTE: We're assuming these will not try to call Break or SetStepping.
  m_system.GetFifo().EmulatorState(running);
  // Core is responsible for shutting down the sound stream.
  if (m_state != State::PowerDown)
    AudioCommon::SetSoundStreamRunning(m_system, running);
}

void CPUManager::Stop()
{
  // Change state and wait for it to be acknowledged.
  // We don't need the stepping lock because State::PowerDown is a priority state which
  // will stick permanently.
  std::unique_lock state_lock(m_state_change_lock);
  m_state = State::PowerDown;
  m_state_cpu_cvar.notify_one();

  while (m_state_cpu_thread_active)
  {
    m_state_cpu_idle_cvar.wait(state_lock);
  }

  RunAdjacentSystems(false);
  FlushStepSyncEventLocked();
}

bool CPUManager::IsStepping() const
{
  return m_state == State::Stepping;
}

State CPUManager::GetState() const
{
  return m_state;
}

const State* CPUManager::GetStatePtr() const
{
  return &m_state;
}

void CPUManager::Reset()
{
}

void CPUManager::StepOpcode(Common::Event* event)
{
  std::lock_guard state_lock(m_state_change_lock);
  // If we're not stepping then this is pointless
  if (!IsStepping())
  {
    if (event)
      event->Set();
    return;
  }

  // Potential race where the previous step has not been serviced yet.
  if (m_state_cpu_step_instruction_sync && m_state_cpu_step_instruction_sync != event)
    m_state_cpu_step_instruction_sync->Set();

  m_state_cpu_step_instruction = true;
  m_state_cpu_step_instruction_sync = event;
  m_state_cpu_cvar.notify_one();
}

// Requires m_state_change_lock
bool CPUManager::SetStateLocked(State s)
{
  if (m_state == State::PowerDown)
    return false;
  if (s == State::Stepping)
    m_system.GetPowerPC().GetBreakPoints().ClearTemporary();
  m_state = s;
  return true;
}

void CPUManager::SetStepping(bool stepping)
{
  std::lock_guard stepping_lock(m_stepping_lock);
  std::unique_lock state_lock(m_state_change_lock);

  if (stepping)
  {
    SetStateLocked(State::Stepping);

    while (m_state_cpu_thread_active)
    {
      m_state_cpu_idle_cvar.wait(state_lock);
    }

    RunAdjacentSystems(false);
  }
  else if (SetStateLocked(State::Running))
  {
    m_state_cpu_cvar.notify_one();
    m_time_played_finish_sync.Set();
    RunAdjacentSystems(true);
  }
}

void CPUManager::Break()
{
  std::lock_guard state_lock(m_state_change_lock);

  // If another thread is trying to PauseAndLock then we need to remember this
  // for later to ignore the unpause_on_unlock.
  if (m_state_paused_and_locked)
  {
    m_state_system_request_stepping = true;
    return;
  }

  // We'll deadlock if we synchronize, the CPU may block waiting for our caller to
  // finish resulting in the CPU loop never terminating.
  SetStateLocked(State::Stepping);
  RunAdjacentSystems(false);
}

void CPUManager::Continue()
{
  SetStepping(false);
  Core::NotifyStateChanged(Core::State::Running);
}

bool CPUManager::PauseAndLock()
{
  m_stepping_lock.lock();

  std::unique_lock state_lock(m_state_change_lock);
  m_state_paused_and_locked = true;

  const bool was_unpaused = m_state == State::Running;
  SetStateLocked(State::Stepping);

  while (m_state_cpu_thread_active)
  {
    m_state_cpu_idle_cvar.wait(state_lock);
  }

  state_lock.unlock();

  // NOTE: It would make more sense for Core::DeclareAsCPUThread() to keep a
  //   depth counter instead of being a boolean.
  if (!Core::IsCPUThread())
  {
    m_have_fake_cpu_thread = true;
    Core::DeclareAsCPUThread();
  }

  return was_unpaused;
}

void CPUManager::RestoreStateAndUnlock(const bool unpause_on_unlock)
{
  // Only need the stepping lock for this
  if (m_have_fake_cpu_thread)
  {
    m_have_fake_cpu_thread = false;
    Core::UndeclareAsCPUThread();
  }

  {
    std::lock_guard state_lock(m_state_change_lock);
    if (m_state_system_request_stepping)
    {
      m_state_system_request_stepping = false;
    }
    else if (unpause_on_unlock)
    {
      SetStateLocked(State::Running);
    }
    m_state_paused_and_locked = false;
    m_state_cpu_cvar.notify_one();

    RunAdjacentSystems(m_state == State::Running);
  }
  m_stepping_lock.unlock();
}

void CPUManager::AddCPUThreadJob(Common::MoveOnlyFunction<void()> function)
{
  std::unique_lock state_lock(m_state_change_lock);
  m_pending_jobs.push(std::move(function));
}

}  // namespace CPU
