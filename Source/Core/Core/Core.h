// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Core

// The external interface to the emulator core. Plus some extras.
// This is another part of the emu that needs cleaning - Core.cpp really has
// too much random junk inside.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"

struct BootParameters;
struct WindowSystemInfo;

namespace Core
{
class System;

bool GetIsThrottlerTempDisabled();
void SetIsThrottlerTempDisabled(bool disable);

// Returns the latest emulation speed (1 is full speed) (swings a lot)
double GetActualEmulationSpeed();

void Callback_FramePresented(double actual_emulation_speed = 1.0);
void Callback_NewField(Core::System& system);

enum class State
{
  Uninitialized,
  Paused,
  Running,
  Stopping,
  Starting,
};

// Console type values based on:
//  - YAGCD 4.2.1.1.2
//  - OSInit (GameCube ELF from Finding Nemo)
//  - OSReportInfo (Wii ELF from Rayman Raving Rabbids)
enum class ConsoleType : u32
{
  // 0x0XXXXXXX Retail units - Gamecube
  HW1 = 1,
  HW2 = 2,
  LatestProductionBoard = 3,
  Reserved = 4,

  // 0x0XXXXXXX Retail units - Wii
  PreProductionBoard0 = 0x10,    // Pre-production board 0
  PreProductionBoard1 = 0x11,    // Pre-production board 1
  PreProductionBoard2_1 = 0x12,  // Pre-production board 2-1
  PreProductionBoard2_2 = 0x20,  // Pre-production board 2-2
  RVL_Retail1 = 0x21,
  RVL_Retail2 = 0x22,
  RVL_Retail3 = 0x23,
  RVA1 = 0x100,  // Revolution Arcade

  // 0x1XXXXXXX Devkits - Gamecube
  // Emulators
  MacEmulator = 0x10000000,  // Mac Emulator
  PcEmulator = 0x10000001,   // PC Emulator

  // Embedded PowerPC series
  Arthur = 0x10000002,  // EPPC Arthur
  Minnow = 0x10000003,  // EPPC Minnow

  // Development HW
  // Version = (console_type & 0x0fffffff) - 3
  FirstDevkit = 0x10000004,
  SecondDevkit = 0x10000005,
  LatestDevkit = 0x10000006,
  ReservedDevkit = 0x10000007,

  // 0x1XXXXXXX Devkits - Wii
  RevolutionEmulator = 0x10000008,  // Revolution Emulator
  NDEV1_0 = 0x10000010,             // NDEV 1.0
  NDEV1_1 = 0x10000011,             // NDEV 1.1
  NDEV1_2 = 0x10000012,             // NDEV 1.2
  NDEV2_0 = 0x10000020,             // NDEV 2.0
  NDEV2_1 = 0x10000021,             // NDEV 2.1

  // 0x2XXXXXXX TDEV-based emulation HW
  // Version = (console_type & 0x0fffffff) - 3
  HW2TDEVSystem = 0x20000005,
  LatestTDEVSystem = 0x20000006,
  ReservedTDEVSystem = 0x20000007,
};

// This is an RAII alternative to using PauseAndLock. If constructed from the host thread, the CPU
// thread is paused, and the current thread temporarily becomes the CPU thread. If constructed from
// the CPU thread, nothing special happens. This should only be constructed on the CPU thread or the
// host thread.
//
// Some functions use a parameter of this type to indicate that the function should only be called
// from the CPU thread. If the parameter is a pointer, the function has a fallback for being called
// from the wrong thread (with the argument being set to nullptr).
class CPUThreadGuard final
{
public:
  explicit CPUThreadGuard(Core::System& system);
  ~CPUThreadGuard();

  CPUThreadGuard(const CPUThreadGuard&) = delete;
  CPUThreadGuard(CPUThreadGuard&&) = delete;
  CPUThreadGuard& operator=(const CPUThreadGuard&) = delete;
  CPUThreadGuard& operator=(CPUThreadGuard&&) = delete;

  Core::System& GetSystem() const { return m_system; }

private:
  Core::System& m_system;
  const bool m_was_cpu_thread;
  const bool m_has_cpu_thread;
  bool m_was_unpaused = false;
};

bool Init(Core::System& system, std::unique_ptr<BootParameters> boot, const WindowSystemInfo& wsi);
void Stop(Core::System& system);
void Shutdown(Core::System& system);

void DeclareAsCPUThread();
void UndeclareAsCPUThread();
void DeclareAsGPUThread();
void UndeclareAsGPUThread();
void DeclareAsHostThread();
void UndeclareAsHostThread();

std::string StopMessage(bool main_thread, std::string_view message);

bool IsRunning(Core::System& system);
bool IsRunningOrStarting(Core::System& system);
bool IsCPUThread();  // this tells us whether we are the CPU thread.
bool IsGPUThread();
bool IsHostThread();

bool WantsDeterminism();

// [NOT THREADSAFE] For use by Host only
void SetState(Core::System& system, State state, bool report_state_change = true,
              bool initial_execution_state = false);
State GetState(Core::System& system);

void SaveScreenShot();
void SaveScreenShot(std::string_view name);

// This displays messages in a user-visible way.
void DisplayMessage(std::string message, int time_in_ms);

void FrameUpdateOnCPUThread();
void OnFrameEnd(Core::System& system);

// Run a function on the CPU thread, asynchronously.
// This is only valid to call from the host thread, since it uses PauseAndLock() internally.
void RunOnCPUThread(Core::System& system, std::function<void()> function, bool wait_for_completion);

// for calling back into UI code without introducing a dependency on it in core
using StateChangedCallbackFunc = std::function<void(Core::State)>;
// Returns a handle
int AddOnStateChangedCallback(StateChangedCallbackFunc callback);
// Also invalidates the handle
bool RemoveOnStateChangedCallback(int* handle);
void CallOnStateChangedCallbacks(Core::State state);

// Run on the Host thread when the factors change. [NOT THREADSAFE]
void UpdateWantDeterminism(Core::System& system, bool initial = false);

// Queue an arbitrary function to asynchronously run once on the Host thread later.
// Threadsafe. Can be called by any thread, including the Host itself.
// Jobs will be executed in RELATIVE order. If you queue 2 jobs from the same thread
// then they will be executed in the order they were queued; however, there is no
// global order guarantee across threads - jobs from other threads may execute in
// between.
// NOTE: Make sure the jobs check the global state instead of assuming everything is
//   still the same as when the job was queued.
// NOTE: Jobs that are not set to run during stop will be discarded instead.
void QueueHostJob(std::function<void(Core::System&)> job, bool run_during_stop = false);

// Should be called periodically by the Host to run pending jobs.
// WMUserJobDispatch will be sent when something is added to the queue.
void HostDispatchJobs(Core::System& system);

void DoFrameStep(Core::System& system);

void UpdateInputGate(bool require_focus, bool require_full_focus = false);

void UpdateTitle(Core::System& system);

}  // namespace Core
