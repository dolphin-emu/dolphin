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

namespace SystemTimers
{
namespace
{
CoreTiming::EventType* et_Dec;
CoreTiming::EventType* et_VI;
CoreTiming::EventType* et_AudioDMA;
CoreTiming::EventType* et_DSP;
CoreTiming::EventType* et_IPC_HLE;
// PatchEngine updates every 1/60th of a second by default
CoreTiming::EventType* et_PatchEngine;
CoreTiming::EventType* et_Throttle;

u32 s_cpu_core_clock = 486000000u;  // 486 mhz (its not 485, stop bugging me!)

// This is completely arbitrary. If we find that we need lower latency,
// we can just increase this number.
int s_ipc_hle_period;

// Custom RTC
s64 s_localtime_rtc_offset = 0;

// For each emulated milliseconds, what was the real time timestamp (excluding sleep time). This is
// a "special" ring buffer where we only need to read the first and last value.
std::array<u64, 1000> s_emu_to_real_time_ring_buffer;
size_t s_emu_to_real_time_index;
std::mutex s_emu_to_real_time_mutex;

// How much time was spent sleeping since the emulator started. Note: this does not need to be reset
// at initialization (or ever), since only the "derivative" of that value really matters.
u64 s_time_spent_sleeping;

// DSP/CPU timeslicing.
void DSPCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  // splits up the cycle budget in case lle is used
  // for hle, just gives all of the slice to hle
  DSP::UpdateDSPSlice(static_cast<int>(DSP::GetDSPEmulator()->DSP_UpdateRate() - cyclesLate));
  system.GetCoreTiming().ScheduleEvent(DSP::GetDSPEmulator()->DSP_UpdateRate() - cyclesLate,
                                       et_DSP);
}

int GetAudioDMACallbackPeriod()
{
  // System internal sample rate is fixed at 32KHz * 4 (16bit Stereo) / 32 bytes DMA
  return static_cast<u64>(s_cpu_core_clock) * AudioInterface::GetAIDSampleRateDivisor() /
         (Mixer::FIXED_SAMPLE_RATE_DIVIDEND * 4 / 32);
}

void AudioDMACallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  DSP::UpdateAudioDMA();  // Push audio to speakers.
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

void VICallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  auto& core_timing = system.GetCoreTiming();
  VideoInterface::Update(core_timing.GetTicks() - cyclesLate);
  core_timing.ScheduleEvent(VideoInterface::GetTicksPerHalfLine() - cyclesLate, et_VI);
}

void DecrementerCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  PowerPC::ppcState.spr[SPR_DEC] = 0xFFFFFFFF;
  PowerPC::ppcState.Exceptions |= EXCEPTION_DECREMENTER;
}

void PatchEngineCallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  // We have 2 periods, a 1000 cycle error period and the VI period.
  // We have to carefully combine these together so that we stay on the VI period without drifting.
  u32 vi_interval = VideoInterface::GetTicksPerField();
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

void ThrottleCallback(Core::System& system, u64 deadline, s64 cyclesLate)
{
  // Allow the GPU thread to sleep. Setting this flag here limits the wakeups to 1 kHz.
  Fifo::GpuMaySleep();

  const u64 time = Common::Timer::NowUs();

  if (deadline == 0)
    deadline = time;

  const s64 diff = deadline - time;
  const float emulation_speed = Config::Get(Config::MAIN_EMULATION_SPEED);
  const bool frame_limiter = emulation_speed > 0.0f && !Core::GetIsThrottlerTempDisabled();
  u32 next_event = GetTicksPerSecond() / 1000;

  {
    std::lock_guard lk(s_emu_to_real_time_mutex);
    s_emu_to_real_time_ring_buffer[s_emu_to_real_time_index] = time - s_time_spent_sleeping;
    s_emu_to_real_time_index =
        (s_emu_to_real_time_index + 1) % s_emu_to_real_time_ring_buffer.size();
  }

  if (frame_limiter)
  {
    if (emulation_speed != 1.0f)
      next_event = u32(next_event * emulation_speed);
    const s64 max_fallback = Config::Get(Config::MAIN_TIMING_VARIANCE) * 1000;
    if (std::abs(diff) > max_fallback)
    {
      DEBUG_LOG_FMT(COMMON, "system too {}, {} us skipped", diff < 0 ? "slow" : "fast",
                    std::abs(diff) - max_fallback);
      deadline = time - max_fallback;
    }
    else if (diff > 1000)
    {
      Common::SleepCurrentThread(diff / 1000);
      s_time_spent_sleeping += Common::Timer::NowUs() - time;
    }
  }
  // reschedule 1ms (possibly scaled by emulation_speed) into future on ppc
  // add 1ms to the deadline
  system.GetCoreTiming().ScheduleEvent(next_event - cyclesLate, et_Throttle, deadline + 1000);
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

  u32 decValue = PowerPC::ppcState.spr[SPR_DEC];

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
  core_timing.SetFakeTBStartValue(PowerPC::ReadFullTimeBaseValue());
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
  u64 ts_now, ts_before;  // In microseconds
  {
    std::lock_guard lk(s_emu_to_real_time_mutex);
    size_t index_now = s_emu_to_real_time_index == 0 ? s_emu_to_real_time_ring_buffer.size() - 1 :
                                                       s_emu_to_real_time_index - 1;
    size_t index_before = s_emu_to_real_time_index;

    ts_now = s_emu_to_real_time_ring_buffer[index_now];
    ts_before = s_emu_to_real_time_ring_buffer[index_before];
  }

  if (ts_before == 0)
  {
    // Not enough data yet to estimate. We could technically provide an estimate based on a shorter
    // time horizon, but it's not really worth it.
    return 1.0;
  }

  u64 delta_us = ts_now - ts_before;
  double emulated_us = s_emu_to_real_time_ring_buffer.size() * 1000.0;  // For each emulated ms.
  return delta_us == 0 ? DBL_MAX : emulated_us / delta_us;
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

  core_timing.SetFakeTBStartValue(static_cast<u64>(s_cpu_core_clock / TIMER_RATIO) *
                                  static_cast<u64>(ExpansionInterface::CEXIIPL::GetEmulatedTime(
                                      ExpansionInterface::CEXIIPL::GC_EPOCH)));

  core_timing.SetFakeTBStartTicks(core_timing.GetTicks());

  core_timing.SetFakeDecStartValue(0xFFFFFFFF);
  core_timing.SetFakeDecStartTicks(core_timing.GetTicks());

  et_Dec = core_timing.RegisterEvent("DecCallback", DecrementerCallback);
  et_VI = core_timing.RegisterEvent("VICallback", VICallback);
  et_DSP = core_timing.RegisterEvent("DSPCallback", DSPCallback);
  et_AudioDMA = core_timing.RegisterEvent("AudioDMACallback", AudioDMACallback);
  et_IPC_HLE = core_timing.RegisterEvent("IPC_HLE_UpdateCallback", IPC_HLE_UpdateCallback);
  et_PatchEngine = core_timing.RegisterEvent("PatchEngine", PatchEngineCallback);
  et_Throttle = core_timing.RegisterEvent("Throttle", ThrottleCallback);

  core_timing.ScheduleEvent(VideoInterface::GetTicksPerHalfLine(), et_VI);
  core_timing.ScheduleEvent(0, et_DSP);
  core_timing.ScheduleEvent(GetAudioDMACallbackPeriod(), et_AudioDMA);
  core_timing.ScheduleEvent(0, et_Throttle, 0);

  core_timing.ScheduleEvent(VideoInterface::GetTicksPerField(), et_PatchEngine);

  if (SConfig::GetInstance().bWii)
    core_timing.ScheduleEvent(s_ipc_hle_period, et_IPC_HLE);

  s_emu_to_real_time_ring_buffer.fill(0);
}

void Shutdown()
{
  Common::Timer::RestoreResolution();
  s_localtime_rtc_offset = 0;
}

}  // namespace SystemTimers
