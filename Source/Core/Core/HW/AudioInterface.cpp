// Copyright 2003 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

/*
Here is a nice ascii overview of audio flow affected by this file:

(RAM)---->[AI FIFO]---->[SRC]---->[Mixer]---->[DAC]---->(Speakers)
                          ^
                          |
                      [L/R Volume]
                           \
(DVD)---->[Drive I/F]---->[SRC]---->[Counter]

Notes:
Output at "48KHz" is actually 48043Hz.
Sample counter counts streaming stereo samples after upsampling.
[DAC] causes [AI I/F] to read from RAM at rate selected by AIDFR.
Each [SRC] will upsample a 32KHz source, or pass through the 48KHz
  source. The [Mixer]/[DAC] only operate at 48KHz.

AIS == disc streaming == DTK(Disk Track Player) == streaming audio, etc.

Supposedly, the retail hardware only supports 48KHz streaming from
  [Drive I/F]. However it's more likely that the hardware supports
  32KHz streaming, and the upsampling is transparent to the user.
  TODO check if anything tries to stream at 32KHz.

The [Drive I/F] actually supports simultaneous requests for audio and
  normal data. For this reason, we can't really get rid of the crit section.

IMPORTANT:
This file mainly deals with the [Drive I/F], however [AIDFR] controls
  the rate at which the audio data is DMA'd from RAM into the [AI FIFO]
  (and the speed at which the FIFO is read by its SRC). Everything else
  relating to AID happens in DSP.cpp. It's kinda just bad hardware design.
  TODO maybe the files should be merged?
*/

#include "Core/HW/AudioInterface.h"

#include <algorithm>

#include "AudioCommon/AudioCommon.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/CoreTiming.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/PowerPC.h"

namespace AudioInterface
{
// Internal hardware addresses
enum
{
  AI_CONTROL_REGISTER = 0x6C00,
  AI_VOLUME_REGISTER = 0x6C04,
  AI_SAMPLE_COUNTER = 0x6C08,
  AI_INTERRUPT_TIMING = 0x6C0C,
};

enum
{
  AIS_32KHz = 0,
  AIS_48KHz = 1,

  AID_32KHz = 1,
  AID_48KHz = 0
};

// AI Control Register
union AICR
{
  AICR() = default;
  explicit AICR(u32 hex_) : hex{hex_} {}
  struct
  {
    u32 PSTAT : 1;     // sample counter/playback enable
    u32 AISFR : 1;     // AIS Frequency (0=32khz 1=48khz)
    u32 AIINTMSK : 1;  // 0=interrupt masked 1=interrupt enabled
    u32 AIINT : 1;     // audio interrupt status
    u32 AIINTVLD : 1;  // This bit controls whether AIINT is affected by the Interrupt Timing
                       // register
                       // matching the sample counter. Once set, AIINT will hold its last value
    u32 SCRESET : 1;   // write to reset counter
    u32 AIDFR : 1;     // AID Frequency (0=48khz 1=32khz)
    u32 : 25;
  };
  u32 hex = 0;
};

// AI Volume Register
union AIVR
{
  struct
  {
    u32 left : 8;
    u32 right : 8;
    u32 : 16;
  };
  u32 hex = 0;
};

// STATE_TO_SAVE
// Registers
static AICR s_control;
static AIVR s_volume;
static u32 s_sample_counter = 0;
static u32 s_interrupt_timing = 0;

static u64 s_last_cpu_time = 0;
static u64 s_cpu_cycles_per_sample = 0xFFFFFFFFFFFULL;

static u32 s_ais_sample_rate = 48000;
static u32 s_aid_sample_rate = 32000;

void DoState(PointerWrap& p)
{
  p.DoPOD(s_control);
  p.DoPOD(s_volume);
  p.Do(s_sample_counter);
  p.Do(s_interrupt_timing);
  p.Do(s_last_cpu_time);
  p.Do(s_ais_sample_rate);
  p.Do(s_aid_sample_rate);
  p.Do(s_cpu_cycles_per_sample);

  g_sound_stream->GetMixer()->DoState(p);
}

static void GenerateAudioInterrupt();
static void UpdateInterrupts();
static void IncreaseSampleCount(u32 amount);
static int GetAIPeriod();
static void Update(u64 userdata, s64 cycles_late);

static CoreTiming::EventType* event_type_ai;

void Init()
{
  s_control.hex = 0;
  s_control.AISFR = AIS_48KHz;
  s_volume.hex = 0;
  s_sample_counter = 0;
  s_interrupt_timing = 0;

  s_last_cpu_time = 0;
  s_cpu_cycles_per_sample = 0xFFFFFFFFFFFULL;

  s_ais_sample_rate = 48000;
  s_aid_sample_rate = 32000;

  event_type_ai = CoreTiming::RegisterEvent("AICallback", Update);
}

void Shutdown()
{
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  mmio->Register(
      base | AI_CONTROL_REGISTER, MMIO::DirectRead<u32>(&s_control.hex),
      MMIO::ComplexWrite<u32>([](u32, u32 val) {
        const AICR tmp_ai_ctrl(val);

        if (s_control.AIINTMSK != tmp_ai_ctrl.AIINTMSK)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "Change AIINTMSK to %d", tmp_ai_ctrl.AIINTMSK);
          s_control.AIINTMSK = tmp_ai_ctrl.AIINTMSK;
        }

        if (s_control.AIINTVLD != tmp_ai_ctrl.AIINTVLD)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "Change AIINTVLD to %d", tmp_ai_ctrl.AIINTVLD);
          s_control.AIINTVLD = tmp_ai_ctrl.AIINTVLD;
        }

        // Set frequency of streaming audio
        if (tmp_ai_ctrl.AISFR != s_control.AISFR)
        {
          // AISFR rates below are intentionally inverted wrt yagcd
          DEBUG_LOG(AUDIO_INTERFACE, "Change AISFR to %s", tmp_ai_ctrl.AISFR ? "48khz" : "32khz");
          s_control.AISFR = tmp_ai_ctrl.AISFR;
          if (SConfig::GetInstance().bWii)
            s_ais_sample_rate = tmp_ai_ctrl.AISFR ? 48000 : 32000;
          else
            s_ais_sample_rate = tmp_ai_ctrl.AISFR ? 48043 : 32029;
          g_sound_stream->GetMixer()->SetStreamInputSampleRate(s_ais_sample_rate);
          s_cpu_cycles_per_sample = SystemTimers::GetTicksPerSecond() / s_ais_sample_rate;
        }
        // Set frequency of DMA
        if (tmp_ai_ctrl.AIDFR != s_control.AIDFR)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "Change AIDFR to %s", tmp_ai_ctrl.AIDFR ? "32khz" : "48khz");
          s_control.AIDFR = tmp_ai_ctrl.AIDFR;
          if (SConfig::GetInstance().bWii)
            s_aid_sample_rate = tmp_ai_ctrl.AIDFR ? 32000 : 48000;
          else
            s_aid_sample_rate = tmp_ai_ctrl.AIDFR ? 32029 : 48043;
          g_sound_stream->GetMixer()->SetDMAInputSampleRate(s_aid_sample_rate);
        }

        // Streaming counter
        if (tmp_ai_ctrl.PSTAT != s_control.PSTAT)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "%s streaming audio", tmp_ai_ctrl.PSTAT ? "start" : "stop");
          s_control.PSTAT = tmp_ai_ctrl.PSTAT;
          s_last_cpu_time = CoreTiming::GetTicks();

          CoreTiming::RemoveEvent(event_type_ai);
          CoreTiming::ScheduleEvent(GetAIPeriod(), event_type_ai);
        }

        // AI Interrupt
        if (tmp_ai_ctrl.AIINT)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "Clear AIS Interrupt");
          s_control.AIINT = 0;
        }

        // Sample Count Reset
        if (tmp_ai_ctrl.SCRESET)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "Reset AIS sample counter");
          s_sample_counter = 0;

          s_last_cpu_time = CoreTiming::GetTicks();
        }

        UpdateInterrupts();
      }));

  mmio->Register(base | AI_VOLUME_REGISTER, MMIO::DirectRead<u32>(&s_volume.hex),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   s_volume.hex = val;
                   g_sound_stream->GetMixer()->SetStreamingVolume(s_volume.left, s_volume.right);
                 }));

  mmio->Register(base | AI_SAMPLE_COUNTER, MMIO::ComplexRead<u32>([](u32) {
                   return s_sample_counter +
                          static_cast<u32>((CoreTiming::GetTicks() - s_last_cpu_time) /
                                           s_cpu_cycles_per_sample);
                 }),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   s_sample_counter = val;
                   s_last_cpu_time = CoreTiming::GetTicks();
                   CoreTiming::RemoveEvent(event_type_ai);
                   CoreTiming::ScheduleEvent(GetAIPeriod(), event_type_ai);
                 }));

  mmio->Register(base | AI_INTERRUPT_TIMING, MMIO::DirectRead<u32>(&s_interrupt_timing),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   DEBUG_LOG(AUDIO_INTERFACE, "AI_INTERRUPT_TIMING=%08x@%08x", val,
                             PowerPC::ppcState.pc);
                   s_interrupt_timing = val;
                   CoreTiming::RemoveEvent(event_type_ai);
                   CoreTiming::ScheduleEvent(GetAIPeriod(), event_type_ai);
                 }));
}

static void UpdateInterrupts()
{
  ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_AI,
                                   s_control.AIINT & s_control.AIINTMSK);
}

static void GenerateAudioInterrupt()
{
  s_control.AIINT = 1;
  UpdateInterrupts();
}

void GenerateAISInterrupt()
{
  GenerateAudioInterrupt();
}

static void IncreaseSampleCount(const u32 amount)
{
  if (s_control.PSTAT)
  {
    const u32 old_sample_counter = s_sample_counter + 1;
    s_sample_counter += amount;

    if ((s_interrupt_timing - old_sample_counter) <= (s_sample_counter - old_sample_counter))
    {
      DEBUG_LOG(AUDIO_INTERFACE, "GenerateAudioInterrupt %08x:%08x @ %08x s_control.AIINTVLD=%d",
                s_sample_counter, s_interrupt_timing, PowerPC::ppcState.pc, s_control.AIINTVLD);
      GenerateAudioInterrupt();
    }
  }
}

bool IsPlaying()
{
  return (s_control.PSTAT == 1);
}

unsigned int GetAIDSampleRate()
{
  return s_aid_sample_rate;
}

static void Update(u64 userdata, s64 cycles_late)
{
  if (s_control.PSTAT)
  {
    const u64 diff = CoreTiming::GetTicks() - s_last_cpu_time;
    if (diff > s_cpu_cycles_per_sample)
    {
      const u32 samples = static_cast<u32>(diff / s_cpu_cycles_per_sample);
      s_last_cpu_time += samples * s_cpu_cycles_per_sample;
      IncreaseSampleCount(samples);
    }
    CoreTiming::ScheduleEvent(GetAIPeriod() - cycles_late, event_type_ai);
  }
}

int GetAIPeriod()
{
  u64 period = s_cpu_cycles_per_sample * (s_interrupt_timing - s_sample_counter);
  u64 s_period = s_cpu_cycles_per_sample * s_ais_sample_rate;
  if (period == 0)
    return static_cast<int>(s_period);
  return static_cast<int>(std::min(period, s_period));
}

}  // end of namespace AudioInterface
