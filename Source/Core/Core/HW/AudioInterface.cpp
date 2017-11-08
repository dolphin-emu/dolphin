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
  AICR() { hex = 0; }
  AICR(u32 _hex) { hex = _hex; }
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
  u32 hex;
};

// AI Volume Register
union AIVR
{
  AIVR() { hex = 0; }
  struct
  {
    u32 left : 8;
    u32 right : 8;
    u32 : 16;
  };
  u32 hex;
};

// STATE_TO_SAVE
// Registers
static AICR m_Control;
static AIVR m_Volume;
static u32 m_SampleCounter = 0;
static u32 m_InterruptTiming = 0;

static u64 g_LastCPUTime = 0;
static u64 g_CPUCyclesPerSample = 0xFFFFFFFFFFFULL;

static unsigned int g_AISSampleRate = 48000;
static unsigned int g_AIDSampleRate = 32000;

void DoState(PointerWrap& p)
{
  p.DoPOD(m_Control);
  p.DoPOD(m_Volume);
  p.Do(m_SampleCounter);
  p.Do(m_InterruptTiming);
  p.Do(g_LastCPUTime);
  p.Do(g_AISSampleRate);
  p.Do(g_AIDSampleRate);
  p.Do(g_CPUCyclesPerSample);

  g_sound_stream->GetMixer()->DoState(p);
}

static void GenerateAudioInterrupt();
static void UpdateInterrupts();
static void IncreaseSampleCount(const u32 _uAmount);
static int GetAIPeriod();
static CoreTiming::EventType* et_AI;
static void Update(u64 userdata, s64 cyclesLate);

void Init()
{
  m_Control.hex = 0;
  m_Control.AISFR = AIS_48KHz;
  m_Volume.hex = 0;
  m_SampleCounter = 0;
  m_InterruptTiming = 0;

  g_LastCPUTime = 0;
  g_CPUCyclesPerSample = 0xFFFFFFFFFFFULL;

  g_AISSampleRate = 48000;
  g_AIDSampleRate = 32000;

  et_AI = CoreTiming::RegisterEvent("AICallback", Update);
}

void Shutdown()
{
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  mmio->Register(
      base | AI_CONTROL_REGISTER, MMIO::DirectRead<u32>(&m_Control.hex),
      MMIO::ComplexWrite<u32>([](u32, u32 val) {
        AICR tmpAICtrl(val);

        if (m_Control.AIINTMSK != tmpAICtrl.AIINTMSK)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "Change AIINTMSK to %d", tmpAICtrl.AIINTMSK);
          m_Control.AIINTMSK = tmpAICtrl.AIINTMSK;
        }

        if (m_Control.AIINTVLD != tmpAICtrl.AIINTVLD)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "Change AIINTVLD to %d", tmpAICtrl.AIINTVLD);
          m_Control.AIINTVLD = tmpAICtrl.AIINTVLD;
        }

        // Set frequency of streaming audio
        if (tmpAICtrl.AISFR != m_Control.AISFR)
        {
          // AISFR rates below are intentionally inverted wrt yagcd
          DEBUG_LOG(AUDIO_INTERFACE, "Change AISFR to %s", tmpAICtrl.AISFR ? "48khz" : "32khz");
          m_Control.AISFR = tmpAICtrl.AISFR;
          if (SConfig::GetInstance().bWii)
            g_AISSampleRate = tmpAICtrl.AISFR ? 48000 : 32000;
          else
            g_AISSampleRate = tmpAICtrl.AISFR ? 48043 : 32029;
          g_sound_stream->GetMixer()->SetStreamInputSampleRate(g_AISSampleRate);
          g_CPUCyclesPerSample = SystemTimers::GetTicksPerSecond() / g_AISSampleRate;
        }
        // Set frequency of DMA
        if (tmpAICtrl.AIDFR != m_Control.AIDFR)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "Change AIDFR to %s", tmpAICtrl.AIDFR ? "32khz" : "48khz");
          m_Control.AIDFR = tmpAICtrl.AIDFR;
          if (SConfig::GetInstance().bWii)
            g_AIDSampleRate = tmpAICtrl.AIDFR ? 32000 : 48000;
          else
            g_AIDSampleRate = tmpAICtrl.AIDFR ? 32029 : 48043;
          g_sound_stream->GetMixer()->SetDMAInputSampleRate(g_AIDSampleRate);
        }

        // Streaming counter
        if (tmpAICtrl.PSTAT != m_Control.PSTAT)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "%s streaming audio", tmpAICtrl.PSTAT ? "start" : "stop");
          m_Control.PSTAT = tmpAICtrl.PSTAT;
          g_LastCPUTime = CoreTiming::GetTicks();

          CoreTiming::RemoveEvent(et_AI);
          CoreTiming::ScheduleEvent(GetAIPeriod(), et_AI);
        }

        // AI Interrupt
        if (tmpAICtrl.AIINT)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "Clear AIS Interrupt");
          m_Control.AIINT = 0;
        }

        // Sample Count Reset
        if (tmpAICtrl.SCRESET)
        {
          DEBUG_LOG(AUDIO_INTERFACE, "Reset AIS sample counter");
          m_SampleCounter = 0;

          g_LastCPUTime = CoreTiming::GetTicks();
        }

        UpdateInterrupts();
      }));

  mmio->Register(base | AI_VOLUME_REGISTER, MMIO::DirectRead<u32>(&m_Volume.hex),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   m_Volume.hex = val;
                   g_sound_stream->GetMixer()->SetStreamingVolume(m_Volume.left, m_Volume.right);
                 }));

  mmio->Register(base | AI_SAMPLE_COUNTER, MMIO::ComplexRead<u32>([](u32) {
                   return m_SampleCounter +
                          static_cast<u32>((CoreTiming::GetTicks() - g_LastCPUTime) /
                                           g_CPUCyclesPerSample);
                 }),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   m_SampleCounter = val;
                   g_LastCPUTime = CoreTiming::GetTicks();
                   CoreTiming::RemoveEvent(et_AI);
                   CoreTiming::ScheduleEvent(GetAIPeriod(), et_AI);
                 }));

  mmio->Register(base | AI_INTERRUPT_TIMING, MMIO::DirectRead<u32>(&m_InterruptTiming),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   DEBUG_LOG(AUDIO_INTERFACE, "AI_INTERRUPT_TIMING=%08x@%08x", val,
                             PowerPC::ppcState.pc);
                   m_InterruptTiming = val;
                   CoreTiming::RemoveEvent(et_AI);
                   CoreTiming::ScheduleEvent(GetAIPeriod(), et_AI);
                 }));
}

static void UpdateInterrupts()
{
  ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_AI,
                                   m_Control.AIINT & m_Control.AIINTMSK);
}

static void GenerateAudioInterrupt()
{
  m_Control.AIINT = 1;
  UpdateInterrupts();
}

void GenerateAISInterrupt()
{
  GenerateAudioInterrupt();
}

static void IncreaseSampleCount(const u32 _iAmount)
{
  if (m_Control.PSTAT)
  {
    u32 old_SampleCounter = m_SampleCounter + 1;
    m_SampleCounter += _iAmount;

    if ((m_InterruptTiming - old_SampleCounter) <= (m_SampleCounter - old_SampleCounter))
    {
      DEBUG_LOG(AUDIO_INTERFACE, "GenerateAudioInterrupt %08x:%08x @ %08x m_Control.AIINTVLD=%d",
                m_SampleCounter, m_InterruptTiming, PowerPC::ppcState.pc, m_Control.AIINTVLD);
      GenerateAudioInterrupt();
    }
  }
}

bool IsPlaying()
{
  return (m_Control.PSTAT == 1);
}

unsigned int GetAIDSampleRate()
{
  return g_AIDSampleRate;
}

static void Update(u64 userdata, s64 cyclesLate)
{
  if (m_Control.PSTAT)
  {
    const u64 Diff = CoreTiming::GetTicks() - g_LastCPUTime;
    if (Diff > g_CPUCyclesPerSample)
    {
      const u32 Samples = static_cast<u32>(Diff / g_CPUCyclesPerSample);
      g_LastCPUTime += Samples * g_CPUCyclesPerSample;
      IncreaseSampleCount(Samples);
    }
    CoreTiming::ScheduleEvent(GetAIPeriod() - cyclesLate, et_AI);
  }
}

int GetAIPeriod()
{
  u64 period = g_CPUCyclesPerSample * (m_InterruptTiming - m_SampleCounter);
  u64 s_period = g_CPUCyclesPerSample * g_AISSampleRate;
  if (period == 0)
    return static_cast<int>(s_period);
  return static_cast<int>(std::min(period, s_period));
}

}  // end of namespace AudioInterface
