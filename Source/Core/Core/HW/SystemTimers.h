// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace Core
{
class System;
}
namespace CoreTiming
{
struct EventType;
}

namespace SystemTimers
{
/*
GameCube                   MHz
flipper <-> ARAM bus:      81 (DSP)
gekko <-> flipper bus:     162
flipper <-> 1T-SRAM bus:   324
gekko:                     486

These contain some guesses:
Wii                             MHz
hollywood <-> GDDR3 RAM bus:    ??? no idea really
broadway <-> hollywood bus:     243
hollywood <-> 1T-SRAM bus:      486
broadway:                       729
*/
// Ratio of TB and Decrementer to clock cycles.
// TB clk is 1/4 of BUS clk. And it seems BUS clk is really 1/3 of CPU clk.
// So, ratio is 1 / (1/4 * 1/3 = 1/12) = 12.
// note: ZWW is ok and faster with TIMER_RATIO=8 though.
// !!! POSSIBLE STABLE PERF BOOST HACK THERE !!!

enum
{
  TIMER_RATIO = 12
};

struct TimeBaseTick
{
  constexpr TimeBaseTick() = default;
  constexpr explicit TimeBaseTick(u64 tb_ticks) : cpu_ticks(tb_ticks * TIMER_RATIO) {}
  constexpr operator u64() const { return cpu_ticks; }

  u64 cpu_ticks = 0;
};

enum class Mode
{
  GC,
  Wii,
};

class SystemTimersManager
{
public:
  explicit SystemTimersManager(Core::System& system);
  SystemTimersManager(const SystemTimersManager& other) = delete;
  SystemTimersManager(SystemTimersManager&& other) = delete;
  SystemTimersManager& operator=(const SystemTimersManager& other) = delete;
  SystemTimersManager& operator=(SystemTimersManager&& other) = delete;
  ~SystemTimersManager();

  u32 GetTicksPerSecond() const;
  void PreInit();
  void Init();
  void Shutdown();
  void ChangePPCClock(Mode mode);

  // Notify timing system that somebody wrote to the decrementer
  void DecrementerSet();
  u32 GetFakeDecrementer() const;

  void TimeBaseSet();
  u64 GetFakeTimeBase() const;
  // Custom RTC
  s64 GetLocalTimeRTCOffset() const;

  // Returns an estimate of how fast/slow the emulation is running (excluding throttling induced
  // sleep time). The estimate is computed over the last 1s of emulated time. Example values:
  //
  // - 0.5: the emulator is running at 50% speed (falling behind).
  // - 1.0: the emulator is running at 100% speed.
  // - 2.0: the emulator is running at 200% speed (or 100% speed but sleeping half of the time).
  double GetEstimatedEmulationPerformance() const;

private:
  static void DSPCallback(Core::System& system, u64 userdata, s64 cycles_late);
  static void AudioDMACallback(Core::System& system, u64 userdata, s64 cycles_late);
  static void IPC_HLE_UpdateCallback(Core::System& system, u64 userdata, s64 cycles_late);
  static void GPUSleepCallback(Core::System& system, u64 userdata, s64 cycles_late);
  static void PerfTrackerCallback(Core::System& system, u64 userdata, s64 cycles_late);
  static void VICallback(Core::System& system, u64 userdata, s64 cycles_late);
  static void DecrementerCallback(Core::System& system, u64 userdata, s64 cycles_late);
  static void PatchEngineCallback(Core::System& system, u64 userdata, s64 cycles_late);

  Core::System& m_system;

  u32 m_cpu_core_clock = 486000000u;  // 486 mhz

  // This is completely arbitrary. If we find that we need lower latency,
  // we can just increase this number.
  int m_ipc_hle_period = 0;

  // Custom RTC
  s64 m_localtime_rtc_offset = 0;

  CoreTiming::EventType* m_event_type_decrementer = nullptr;
  CoreTiming::EventType* m_event_type_vi = nullptr;
  CoreTiming::EventType* m_event_type_audio_dma = nullptr;
  CoreTiming::EventType* m_event_type_dsp = nullptr;
  CoreTiming::EventType* m_event_type_ipc_hle = nullptr;
  CoreTiming::EventType* m_event_type_gpu_sleeper = nullptr;
  CoreTiming::EventType* m_event_type_perf_tracker = nullptr;
  // PatchEngine updates every 1/60th of a second by default
  CoreTiming::EventType* m_event_type_patch_engine = nullptr;
};
}  // namespace SystemTimers

inline namespace SystemTimersLiterals
{
/// Converts timebase ticks to clock ticks.
constexpr SystemTimers::TimeBaseTick operator""_tbticks(unsigned long long value)
{
  return SystemTimers::TimeBaseTick(value);
}
}  // namespace SystemTimersLiterals
