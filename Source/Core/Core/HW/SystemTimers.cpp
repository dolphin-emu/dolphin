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
#include "Core/ConfigManager.h"
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
namespace
{
CoreTiming::EventType* et_Dec;
CoreTiming::EventType* et_VI;
CoreTiming::EventType* et_AudioDMA;
CoreTiming::EventType* et_DSP;
CoreTiming::EventType* et_IPC_HLE;
CoreTiming::EventType* et_GPU_sleeper;
CoreTiming::EventType* et_perf_tracker;
// PatchEngine updates every 1/60th of a second by default
CoreTiming::EventType* et_PatchEngine;

u32 s_cpu_core_clock = 486000000u;  // 486 mhz (its not 485, stop bugging me!)

// This is completely arbitrary. If we find that we need lower latency,
// we can just increase this number.
int s_ipc_hle_period;

// Custom RTC
s64 s_localtime_rtc_offset = 0;

// DSP/CPU timeslicing.
void DSPCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  // splits up the cycle budget in case lle is used
  // for hle, just gives all of the slice to hle
  auto& dsp = system.GetDSP();
  dsp.UpdateDSPSlice(static_cast<int>(dsp.GetDSPEmulator()->DSP_UpdateRate() - cyclesLate));
  system.GetCoreTiming().ScheduleEvent(dsp.GetDSPEmulator()->DSP_UpdateRate() - cyclesLate, et_DSP);
}

int GetAudioDMACallbackPeriod()
{
  // System internal sample rate is fixed at 32KHz * 4 (16bit Stereo) / 32 bytes DMA
  auto& system = Core::System::GetInstance();
  return static_cast<u64>(s_cpu_core_clock) * system.GetAudioInterface().GetAIDSampleRateDivisor() /
         (Mixer::FIXED_SAMPLE_RATE_DIVIDEND * 4 / 32);
}

void AudioDMACallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  system.GetDSP().UpdateAudioDMA();  // Push audio to speakers.
  system.GetCoreTiming().ScheduleEvent(GetAudioDMACallbackPeriod() - cyclesLate, et_AudioDMA);
}

void IPC_HLE_UpdateCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  if (SConfig::GetInstance().bWii)
  {
    IOS::HLE::GetIOS()->UpdateDevices();
    system.GetCoreTiming().ScheduleEvent(s_ipc_hle_period - cyclesLate, et_IPC_HLE);
  }
}

void GPUSleepCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  auto& core_timing = system.GetCoreTiming();
  system.GetFifo().GpuMaySleep();

  // We want to call GpuMaySleep at about 1000hz so
  // that the thread can sleep while not doing anything.
  core_timing.ScheduleEvent(GetTicksPerSecond() / 1000 - cyclesLate, et_GPU_sleeper);
}

void PerfTrackerCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  auto& core_timing = system.GetCoreTiming();
  g_perf_metrics.CountPerformanceMarker(system, cyclesLate);

  // Call this performance tracker again in 1/100th of a second.
  // The tracker stores 256 values so this will let us summarize the last 2.56 seconds.
  // The performance metrics require this to be called at 100hz for the speed% is correct.
  core_timing.ScheduleEvent(GetTicksPerSecond() / 100 - cyclesLate, et_perf_tracker);
}

void VICallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  auto& core_timing = system.GetCoreTiming();
  auto& vi = system.GetVideoInterface();
  vi.Update(core_timing.GetTicks() - cyclesLate);
  core_timing.ScheduleEvent(vi.GetTicksPerHalfLine() - cyclesLate, et_VI);
}

void DecrementerCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  auto& ppc_state = system.GetPPCState();
  ppc_state.spr[SPR_DEC] = 0xFFFFFFFF;
  ppc_state.Exceptions |= EXCEPTION_DECREMENTER;
}

void PatchEngineCallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  // We have 2 periods, a 1000 cycle error period and the VI period.
  // We have to carefully combine these together so that we stay on the VI period without drifting.
  u32 vi_interval = system.GetVideoInterface().GetTicksPerField();
  s64 cycles_pruned = (userdata + cycles_late) % vi_interval;
  s64 next_schedule = 0;

  // Try to patch mem and run the Action Replay
  if (PatchEngine::ApplyFramePatches())
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

  system.GetCoreTiming().ScheduleEvent(next_schedule, et_PatchEngine, cycles_pruned);
}
}  // namespace

u32 GetTicksPerSecond()
{
  return s_cpu_core_clock;
}

void DecrementerSet()
{
  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  auto& ppc_state = system.GetPPCState();

  u32 decValue = ppc_state.spr[SPR_DEC];

  core_timing.RemoveEvent(et_Dec);
  if ((decValue & 0x80000000) == 0)
  {
    core_timing.SetFakeDecStartTicks(core_timing.GetTicks());
    core_timing.SetFakeDecStartValue(decValue);

    core_timing.ScheduleEvent(decValue * TIMER_RATIO, et_Dec);
  }
}

u32 GetFakeDecrementer()
{
  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  return (core_timing.GetFakeDecStartValue() -
          (u32)((core_timing.GetTicks() - core_timing.GetFakeDecStartTicks()) / TIMER_RATIO));
}

void TimeBaseSet()
{
  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  core_timing.SetFakeTBStartTicks(core_timing.GetTicks());
  core_timing.SetFakeTBStartValue(system.GetPowerPC().ReadFullTimeBaseValue());
}

u64 GetFakeTimeBase()
{
  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  return core_timing.GetFakeTBStartValue() +
         ((core_timing.GetTicks() - core_timing.GetFakeTBStartTicks()) / TIMER_RATIO);
}

s64 GetLocalTimeRTCOffset()
{
  return s_localtime_rtc_offset;
}

double GetEstimatedEmulationPerformance()
{
  return g_perf_metrics.GetMaxSpeed();
}

// split from Init to break a circular dependency between VideoInterface::Init and
// SystemTimers::Init
void PreInit()
{
  ChangePPCClock(SConfig::GetInstance().bWii ? Mode::Wii : Mode::GC);
}

void ChangePPCClock(Mode mode)
{
  const u32 previous_clock = s_cpu_core_clock;
  if (mode == Mode::Wii)
    s_cpu_core_clock = 729000000u;
  else
    s_cpu_core_clock = 486000000u;
  Core::System::GetInstance().GetCoreTiming().AdjustEventQueueTimes(s_cpu_core_clock,
                                                                    previous_clock);
}

void Init()
{
  if (SConfig::GetInstance().bWii)
  {
    // AyuanX: TO BE TWEAKED
    // Now the 1500 is a pure assumption
    // We need to figure out the real frequency though
    const int freq = 1500;
    s_ipc_hle_period = GetTicksPerSecond() / freq;
  }

  Common::Timer::IncreaseResolution();
  // store and convert localtime at boot to timebase ticks
  if (Config::Get(Config::MAIN_CUSTOM_RTC_ENABLE))
  {
    s_localtime_rtc_offset =
        Common::Timer::GetLocalTimeSinceJan1970() - Config::Get(Config::MAIN_CUSTOM_RTC_VALUE);
  }

  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  auto& vi = system.GetVideoInterface();

  core_timing.SetFakeTBStartValue(static_cast<u64>(s_cpu_core_clock / TIMER_RATIO) *
                                  static_cast<u64>(ExpansionInterface::CEXIIPL::GetEmulatedTime(
                                      system, ExpansionInterface::CEXIIPL::GC_EPOCH)));

  core_timing.SetFakeTBStartTicks(core_timing.GetTicks());

  core_timing.SetFakeDecStartValue(0xFFFFFFFF);
  core_timing.SetFakeDecStartTicks(core_timing.GetTicks());

  et_Dec = core_timing.RegisterEvent("DecCallback", DecrementerCallback);
  et_VI = core_timing.RegisterEvent("VICallback", VICallback);
  et_DSP = core_timing.RegisterEvent("DSPCallback", DSPCallback);
  et_AudioDMA = core_timing.RegisterEvent("AudioDMACallback", AudioDMACallback);
  et_IPC_HLE = core_timing.RegisterEvent("IPC_HLE_UpdateCallback", IPC_HLE_UpdateCallback);
  et_GPU_sleeper = core_timing.RegisterEvent("GPUSleeper", GPUSleepCallback);
  et_perf_tracker = core_timing.RegisterEvent("PerfTracker", PerfTrackerCallback);
  et_PatchEngine = core_timing.RegisterEvent("PatchEngine", PatchEngineCallback);

  core_timing.ScheduleEvent(0, et_perf_tracker);
  core_timing.ScheduleEvent(0, et_GPU_sleeper);
  core_timing.ScheduleEvent(vi.GetTicksPerHalfLine(), et_VI);
  core_timing.ScheduleEvent(0, et_DSP);
  core_timing.ScheduleEvent(GetAudioDMACallbackPeriod(), et_AudioDMA);

  core_timing.ScheduleEvent(vi.GetTicksPerField(), et_PatchEngine);

  if (SConfig::GetInstance().bWii)
    core_timing.ScheduleEvent(s_ipc_hle_period, et_IPC_HLE);
}

void Shutdown()
{
  Common::Timer::RestoreResolution();
  s_localtime_rtc_offset = 0;
}

}  // namespace SystemTimers
