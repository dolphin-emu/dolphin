// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This file controls all system timers

/* (shuffle2) I don't know who wrote this, but take it with salt. For starters, "time" is
contextual...
"Time" is measured in frames, not time: These update frequencies are determined by the passage
of frames. So if a game runs slow, on a slow computer for example, these updates will occur
less frequently. This makes sense because almost all console games are controlled by frames
rather than time, so if a game can't keep up with the normal framerate all animations and
actions slows down and the game runs to slow. This is different from PC games that are
often controlled by time instead and may not have maximum framerates.

However, I'm not sure if the Bluetooth communication for the Wii Remote is entirely frame
dependent, the timing problems with the ack command in Zelda - TP may be related to
time rather than frames? For now the IPC_HLE_PERIOD is frame dependent, but because of
different conditions on the way to PluginWiimote::Wiimote_Update() the updates may actually
be time related after all, or not?

I'm not sure about this but the text below seems to assume that 60 fps means that the game
runs in the normal intended speed. In that case an update time of [GetTicksPerSecond() / 60]
would mean one update per frame and [GetTicksPerSecond() / 250] would mean four updates per
frame.


IPC_HLE_PERIOD: For the Wii Remote this is the call schedule:
  IPC_HLE_UpdateCallback() // In this file

    // This function seems to call all devices' Update() function four times per frame
    IOS::HLE::Update()

      // If the AclFrameQue is empty this will call Wiimote_Update() and make it send
      the current input status to the game. I'm not sure if this occurs approximately
      once every frame or if the frequency is not exactly tied to rendered frames
      IOS::HLE::BluetoothEmuDevice::Update()
      PluginWiimote::Wiimote_Update()

      // This is also a device updated by IOS::HLE::Update() but it doesn't
      seem to ultimately call PluginWiimote::Wiimote_Update(). However it can be called
      by the /dev/usb/oh1 device if the AclFrameQue is empty.
      IOS::HLE::WiimoteDevice::Update()
*/

#include "Core/HW/SystemTimers.h"

#include <cfloat>
#include <cmath>
#include <cstdlib>

#include "AudioCommon/Mixer.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/DSPEmulator.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/DSP.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/VideoInterface.h"
#include "Core/IOS/IOS.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/PerformanceMetrics.h"

namespace SystemTimers
{
// DSP/CPU timeslicing.
void SystemTimersManager::DSPCallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  // splits up the cycle budget in case lle is used
  // for hle, just gives all of the slice to hle
  auto& dsp = system.GetDSP();
  dsp.UpdateDSPSlice(static_cast<int>(dsp.GetDSPEmulator()->DSP_UpdateRate() - cycles_late));
  system.GetCoreTiming().ScheduleEvent(dsp.GetDSPEmulator()->DSP_UpdateRate() - cycles_late,
                                       system.GetSystemTimers().m_event_type_dsp);
}

static int GetAudioDMACallbackPeriod(u32 cpu_core_clock, u32 aid_sample_rate_divisor)
{
  // System internal sample rate is fixed at 32KHz * 4 (16bit Stereo) / 32 bytes DMA
  return static_cast<u64>(cpu_core_clock) * aid_sample_rate_divisor /
         (Mixer::FIXED_SAMPLE_RATE_DIVIDEND * 4 / 32);
}

void SystemTimersManager::AudioDMACallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  system.GetDSP().UpdateAudioDMA();  // Push audio to speakers.
  auto& system_timers = system.GetSystemTimers();
  const int callback_period = GetAudioDMACallbackPeriod(
      system_timers.m_cpu_core_clock, system.GetAudioInterface().GetAIDSampleRateDivisor());
  system.GetCoreTiming().ScheduleEvent(callback_period - cycles_late,
                                       system_timers.m_event_type_audio_dma);
}

void SystemTimersManager::IPC_HLE_UpdateCallback(Core::System& system, u64 userdata,
                                                 s64 cycles_late)
{
  if (system.IsWii())
  {
    system.GetIOS()->UpdateDevices();
    auto& system_timers = system.GetSystemTimers();
    system.GetCoreTiming().ScheduleEvent(system_timers.m_ipc_hle_period - cycles_late,
                                         system_timers.m_event_type_ipc_hle);
  }
}

void SystemTimersManager::GPUSleepCallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  auto& core_timing = system.GetCoreTiming();
  system.GetFifo().GpuMaySleep();

  // We want to call GpuMaySleep at about 1000hz so
  // that the thread can sleep while not doing anything.
  auto& system_timers = system.GetSystemTimers();
  core_timing.ScheduleEvent(system_timers.GetTicksPerSecond() / 1000 - cycles_late,
                            system_timers.m_event_type_gpu_sleeper);
}

void SystemTimersManager::PerfTrackerCallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  auto& core_timing = system.GetCoreTiming();
  g_perf_metrics.CountPerformanceMarker(system, cycles_late);

  // Call this performance tracker again in 1/100th of a second.
  // The tracker stores 256 values so this will let us summarize the last 2.56 seconds.
  // The performance metrics require this to be called at 100hz for the speed% is correct.
  auto& system_timers = system.GetSystemTimers();
  core_timing.ScheduleEvent(system_timers.GetTicksPerSecond() / 100 - cycles_late,
                            system_timers.m_event_type_perf_tracker);
}

void SystemTimersManager::VICallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  auto& core_timing = system.GetCoreTiming();
  auto& vi = system.GetVideoInterface();
  vi.Update(core_timing.GetTicks() - cycles_late);
  core_timing.ScheduleEvent(vi.GetTicksPerHalfLine() - cycles_late,
                            system.GetSystemTimers().m_event_type_vi);
}

void SystemTimersManager::DecrementerCallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  auto& ppc_state = system.GetPPCState();
  ppc_state.spr[SPR_DEC] = 0xFFFFFFFF;
  ppc_state.Exceptions |= EXCEPTION_DECREMENTER;
}

void SystemTimersManager::PatchEngineCallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  // We have 2 periods, a 1000 cycle error period and the VI period.
  // We have to carefully combine these together so that we stay on the VI period without drifting.
  u32 vi_interval = system.GetVideoInterface().GetTicksPerField();
  s64 cycles_pruned = (userdata + cycles_late) % vi_interval;
  s64 next_schedule = 0;

  // Try to patch mem and run the Action Replay
  if (PatchEngine::ApplyFramePatches(system))
  {
    next_schedule = vi_interval - cycles_pruned;
    cycles_pruned = 0;
  }
  else
  {
    // The patch failed, usually because the CPU is in an inappropriate state (interrupt handler).
    // We'll try again after 1000 cycles.
    next_schedule = 1000;
    cycles_pruned += next_schedule;
  }

  system.GetCoreTiming().ScheduleEvent(
      next_schedule, system.GetSystemTimers().m_event_type_patch_engine, cycles_pruned);
}

SystemTimersManager::SystemTimersManager(Core::System& system) : m_system(system)
{
}

SystemTimersManager::~SystemTimersManager() = default;

u32 SystemTimersManager::GetTicksPerSecond() const
{
  return m_cpu_core_clock;
}

void SystemTimersManager::DecrementerSet()
{
  auto& core_timing = m_system.GetCoreTiming();
  auto& ppc_state = m_system.GetPPCState();

  u32 decValue = ppc_state.spr[SPR_DEC];

  core_timing.RemoveEvent(m_event_type_decrementer);
  if ((decValue & 0x80000000) == 0)
  {
    core_timing.SetFakeDecStartTicks(core_timing.GetTicks());
    core_timing.SetFakeDecStartValue(decValue);

    core_timing.ScheduleEvent(decValue * TIMER_RATIO, m_event_type_decrementer);
  }
}

u32 SystemTimersManager::GetFakeDecrementer() const
{
  const auto& core_timing = m_system.GetCoreTiming();
  return (core_timing.GetFakeDecStartValue() -
          (u32)((core_timing.GetTicks() - core_timing.GetFakeDecStartTicks()) / TIMER_RATIO));
}

void SystemTimersManager::TimeBaseSet()
{
  auto& core_timing = m_system.GetCoreTiming();
  core_timing.SetFakeTBStartTicks(core_timing.GetTicks());
  core_timing.SetFakeTBStartValue(m_system.GetPowerPC().ReadFullTimeBaseValue());
}

u64 SystemTimersManager::GetFakeTimeBase() const
{
  const auto& core_timing = m_system.GetCoreTiming();
  return core_timing.GetFakeTBStartValue() +
         ((core_timing.GetTicks() - core_timing.GetFakeTBStartTicks()) / TIMER_RATIO);
}

s64 SystemTimersManager::GetLocalTimeRTCOffset() const
{
  return m_localtime_rtc_offset;
}

double SystemTimersManager::GetEstimatedEmulationPerformance() const
{
  return g_perf_metrics.GetMaxSpeed();
}

// split from Init to break a circular dependency between VideoInterface::Init and
// SystemTimers::Init
void SystemTimersManager::PreInit()
{
  ChangePPCClock(m_system.IsWii() ? Mode::Wii : Mode::GC);
}

void SystemTimersManager::ChangePPCClock(Mode mode)
{
  const u32 previous_clock = m_cpu_core_clock;
  if (mode == Mode::Wii)
    m_cpu_core_clock = 729000000u;
  else
    m_cpu_core_clock = 486000000u;
  m_system.GetCoreTiming().AdjustEventQueueTimes(m_cpu_core_clock, previous_clock);
}

void SystemTimersManager::Init()
{
  if (m_system.IsWii())
  {
    // AyuanX: TO BE TWEAKED
    // Now the 1500 is a pure assumption
    // We need to figure out the real frequency though
    const int freq = 1500;
    m_ipc_hle_period = GetTicksPerSecond() / freq;
  }

  Common::Timer::IncreaseResolution();
  // store and convert localtime at boot to timebase ticks
  if (Config::Get(Config::MAIN_CUSTOM_RTC_ENABLE))
  {
    m_localtime_rtc_offset =
        Common::Timer::GetLocalTimeSinceJan1970() - Config::Get(Config::MAIN_CUSTOM_RTC_VALUE);
  }

  auto& core_timing = m_system.GetCoreTiming();
  auto& vi = m_system.GetVideoInterface();

  core_timing.SetFakeTBStartValue(static_cast<u64>(m_cpu_core_clock / TIMER_RATIO) *
                                  static_cast<u64>(ExpansionInterface::CEXIIPL::GetEmulatedTime(
                                      m_system, ExpansionInterface::CEXIIPL::GC_EPOCH)));

  core_timing.SetFakeTBStartTicks(core_timing.GetTicks());

  core_timing.SetFakeDecStartValue(0xFFFFFFFF);
  core_timing.SetFakeDecStartTicks(core_timing.GetTicks());

  m_event_type_decrementer = core_timing.RegisterEvent("DecCallback", DecrementerCallback);
  m_event_type_vi = core_timing.RegisterEvent("VICallback", VICallback);
  m_event_type_dsp = core_timing.RegisterEvent("DSPCallback", DSPCallback);
  m_event_type_audio_dma = core_timing.RegisterEvent("AudioDMACallback", AudioDMACallback);
  m_event_type_ipc_hle =
      core_timing.RegisterEvent("IPC_HLE_UpdateCallback", IPC_HLE_UpdateCallback);
  m_event_type_gpu_sleeper = core_timing.RegisterEvent("GPUSleeper", GPUSleepCallback);
  m_event_type_perf_tracker = core_timing.RegisterEvent("PerfTracker", PerfTrackerCallback);
  m_event_type_patch_engine = core_timing.RegisterEvent("PatchEngine", PatchEngineCallback);

  core_timing.ScheduleEvent(0, m_event_type_perf_tracker);
  core_timing.ScheduleEvent(0, m_event_type_gpu_sleeper);
  core_timing.ScheduleEvent(vi.GetTicksPerHalfLine(), m_event_type_vi);
  core_timing.ScheduleEvent(0, m_event_type_dsp);

  const int audio_dma_callback_period = GetAudioDMACallbackPeriod(
      m_cpu_core_clock, m_system.GetAudioInterface().GetAIDSampleRateDivisor());
  core_timing.ScheduleEvent(audio_dma_callback_period, m_event_type_audio_dma);

  core_timing.ScheduleEvent(vi.GetTicksPerField(), m_event_type_patch_engine);

  if (m_system.IsWii())
    core_timing.ScheduleEvent(m_ipc_hle_period, m_event_type_ipc_hle);
}

void SystemTimersManager::Shutdown()
{
  Common::Timer::RestoreResolution();
  m_localtime_rtc_offset = 0;
}

}  // namespace SystemTimers
