// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
      IOS::HLE::Device::BluetoothEmu::Update()
      PluginWiimote::Wiimote_Update()

      // This is also a device updated by IOS::HLE::Update() but it doesn't
      seem to ultimately call PluginWiimote::Wiimote_Update(). However it can be called
      by the /dev/usb/oh1 device if the AclFrameQue is empty.
      IOS::HLE::WiimoteDevice::Update()
*/

#include "Core/HW/SystemTimers.h"

#include "Common/Atomic.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
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
#include "VideoCommon/Fifo.h"

namespace SystemTimers
{
static CoreTiming::EventType* et_Dec;
static CoreTiming::EventType* et_VI;
static CoreTiming::EventType* et_AudioDMA;
static CoreTiming::EventType* et_DSP;
static CoreTiming::EventType* et_IPC_HLE;
// PatchEngine updates every 1/60th of a second by default
static CoreTiming::EventType* et_PatchEngine;
static CoreTiming::EventType* et_Throttle;

static u32 s_cpu_core_clock = 486000000u;  // 486 mhz (its not 485, stop bugging me!)

// These two are badly educated guesses.
// Feel free to experiment. Set them in Init below.

// This is a fixed value, don't change it
static int s_audio_dma_period;
// This is completely arbitrary. If we find that we need lower latency,
// we can just increase this number.
static int s_ipc_hle_period;

// Custom RTC
static s64 s_localtime_rtc_offset = 0;

u32 GetTicksPerSecond()
{
  return s_cpu_core_clock;
}

// DSP/CPU timeslicing.
static void DSPCallback(u64 userdata, s64 cyclesLate)
{
  // splits up the cycle budget in case lle is used
  // for hle, just gives all of the slice to hle
  DSP::UpdateDSPSlice(static_cast<int>(DSP::GetDSPEmulator()->DSP_UpdateRate() - cyclesLate));
  CoreTiming::ScheduleEvent(DSP::GetDSPEmulator()->DSP_UpdateRate() - cyclesLate, et_DSP);
}

static void AudioDMACallback(u64 userdata, s64 cyclesLate)
{
  int period = s_cpu_core_clock / (AudioInterface::GetAIDSampleRate() * 4 / 32);
  DSP::UpdateAudioDMA();  // Push audio to speakers.
  CoreTiming::ScheduleEvent(period - cyclesLate, et_AudioDMA);
}

static void IPC_HLE_UpdateCallback(u64 userdata, s64 cyclesLate)
{
  if (SConfig::GetInstance().bWii)
  {
    IOS::HLE::GetIOS()->UpdateDevices();
    CoreTiming::ScheduleEvent(s_ipc_hle_period - cyclesLate, et_IPC_HLE);
  }
}

static void VICallback(u64 userdata, s64 cyclesLate)
{
  VideoInterface::Update(CoreTiming::GetTicks() - cyclesLate);
  CoreTiming::ScheduleEvent(VideoInterface::GetTicksPerHalfLine() - cyclesLate, et_VI);
}

static void DecrementerCallback(u64 userdata, s64 cyclesLate)
{
  PowerPC::ppcState.spr[SPR_DEC] = 0xFFFFFFFF;
  PowerPC::ppcState.Exceptions |= EXCEPTION_DECREMENTER;
}

void DecrementerSet()
{
  u32 decValue = PowerPC::ppcState.spr[SPR_DEC];

  CoreTiming::RemoveEvent(et_Dec);
  if ((decValue & 0x80000000) == 0)
  {
    CoreTiming::SetFakeDecStartTicks(CoreTiming::GetTicks());
    CoreTiming::SetFakeDecStartValue(decValue);

    CoreTiming::ScheduleEvent(decValue * TIMER_RATIO, et_Dec);
  }
}

u32 GetFakeDecrementer()
{
  return (CoreTiming::GetFakeDecStartValue() -
          (u32)((CoreTiming::GetTicks() - CoreTiming::GetFakeDecStartTicks()) / TIMER_RATIO));
}

void TimeBaseSet()
{
  CoreTiming::SetFakeTBStartTicks(CoreTiming::GetTicks());
  CoreTiming::SetFakeTBStartValue(*((u64*)&TL));
}

u64 GetFakeTimeBase()
{
  return CoreTiming::GetFakeTBStartValue() +
         ((CoreTiming::GetTicks() - CoreTiming::GetFakeTBStartTicks()) / TIMER_RATIO);
}

s64 GetLocalTimeRTCOffset()
{
  return s_localtime_rtc_offset;
}

static void PatchEngineCallback(u64 userdata, s64 cycles_late)
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

  CoreTiming::ScheduleEvent(next_schedule, et_PatchEngine, cycles_pruned);
}

static void ThrottleCallback(u64 last_time, s64 cyclesLate)
{
  // Allow the GPU thread to sleep. Setting this flag here limits the wakeups to 1 kHz.
  Fifo::GpuMaySleep();

  u32 time = Common::Timer::GetTimeMs();

  int diff = (u32)last_time - time;
  const SConfig& config = SConfig::GetInstance();
  bool frame_limiter = config.m_EmulationSpeed > 0.0f && !Core::GetIsThrottlerTempDisabled();
  u32 next_event = GetTicksPerSecond() / 1000;
  if (frame_limiter)
  {
    if (config.m_EmulationSpeed != 1.0f)
      next_event = u32(next_event * config.m_EmulationSpeed);
    const int max_fallback = config.iTimingVariance;
    if (abs(diff) > max_fallback)
    {
      DEBUG_LOG(COMMON, "system too %s, %d ms skipped", diff < 0 ? "slow" : "fast",
                abs(diff) - max_fallback);
      last_time = time - max_fallback;
    }
    else if (diff > 0)
      Common::SleepCurrentThread(diff);
  }
  CoreTiming::ScheduleEvent(next_event - cyclesLate, et_Throttle, last_time + 1);
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
  CoreTiming::AdjustEventQueueTimes(s_cpu_core_clock, previous_clock);
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

  // System internal sample rate is fixed at 32KHz * 4 (16bit Stereo) / 32 bytes DMA
  s_audio_dma_period = s_cpu_core_clock / (AudioInterface::GetAIDSampleRate() * 4 / 32);

  Common::Timer::IncreaseResolution();
  // store and convert localtime at boot to timebase ticks
  if (SConfig::GetInstance().bEnableCustomRTC)
  {
    s_localtime_rtc_offset =
        Common::Timer::GetLocalTimeSinceJan1970() - SConfig::GetInstance().m_customRTCValue;
  }

  CoreTiming::SetFakeTBStartValue(static_cast<u64>(s_cpu_core_clock / TIMER_RATIO) *
                                  static_cast<u64>(ExpansionInterface::CEXIIPL::GetEmulatedTime(
                                      ExpansionInterface::CEXIIPL::GC_EPOCH)));

  CoreTiming::SetFakeTBStartTicks(CoreTiming::GetTicks());

  CoreTiming::SetFakeDecStartValue(0xFFFFFFFF);
  CoreTiming::SetFakeDecStartTicks(CoreTiming::GetTicks());

  et_Dec = CoreTiming::RegisterEvent("DecCallback", DecrementerCallback);
  et_VI = CoreTiming::RegisterEvent("VICallback", VICallback);
  et_DSP = CoreTiming::RegisterEvent("DSPCallback", DSPCallback);
  et_AudioDMA = CoreTiming::RegisterEvent("AudioDMACallback", AudioDMACallback);
  et_IPC_HLE = CoreTiming::RegisterEvent("IPC_HLE_UpdateCallback", IPC_HLE_UpdateCallback);
  et_PatchEngine = CoreTiming::RegisterEvent("PatchEngine", PatchEngineCallback);
  et_Throttle = CoreTiming::RegisterEvent("Throttle", ThrottleCallback);

  CoreTiming::ScheduleEvent(VideoInterface::GetTicksPerHalfLine(), et_VI);
  CoreTiming::ScheduleEvent(0, et_DSP);
  CoreTiming::ScheduleEvent(s_audio_dma_period, et_AudioDMA);
  CoreTiming::ScheduleEvent(0, et_Throttle, Common::Timer::GetTimeMs());

  CoreTiming::ScheduleEvent(VideoInterface::GetTicksPerField(), et_PatchEngine);

  if (SConfig::GetInstance().bWii)
    CoreTiming::ScheduleEvent(s_ipc_hle_period, et_IPC_HLE);
}

void Shutdown()
{
  Common::Timer::RestoreResolution();
  s_localtime_rtc_offset = 0;
}

}  // namespace
