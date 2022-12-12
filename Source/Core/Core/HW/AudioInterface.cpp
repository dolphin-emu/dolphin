// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
#include "Core/System.h"

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

enum class SampleRate
{
  AI32KHz,
  AI48KHz,
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

struct AudioInterfaceState::Data
{
  // Registers
  AICR control;
  AIVR volume;

  u32 sample_counter = 0;
  u32 interrupt_timing = 0;

  u64 last_cpu_time = 0;
  u64 cpu_cycles_per_sample = 0;

  u32 ais_sample_rate_divisor = Mixer::FIXED_SAMPLE_RATE_DIVIDEND / 48000;
  u32 aid_sample_rate_divisor = Mixer::FIXED_SAMPLE_RATE_DIVIDEND / 32000;

  CoreTiming::EventType* event_type_ai;
};

AudioInterfaceState::AudioInterfaceState() : m_data(std::make_unique<Data>())
{
}

AudioInterfaceState::~AudioInterfaceState() = default;

void DoState(PointerWrap& p)
{
  auto& system = Core::System::GetInstance();
  auto& state = system.GetAudioInterfaceState().GetData();

  p.Do(state.control);
  p.Do(state.volume);
  p.Do(state.sample_counter);
  p.Do(state.interrupt_timing);
  p.Do(state.last_cpu_time);
  p.Do(state.ais_sample_rate_divisor);
  p.Do(state.aid_sample_rate_divisor);
  p.Do(state.cpu_cycles_per_sample);

  SoundStream* sound_stream = system.GetSoundStream();
  sound_stream->GetMixer()->DoState(p);
}

namespace
{
void UpdateInterrupts()
{
  auto& state = Core::System::GetInstance().GetAudioInterfaceState().GetData();
  ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_AI,
                                   state.control.AIINT & state.control.AIINTMSK);
}

void GenerateAudioInterrupt()
{
  auto& state = Core::System::GetInstance().GetAudioInterfaceState().GetData();
  state.control.AIINT = 1;
  UpdateInterrupts();
}

void IncreaseSampleCount(const u32 amount)
{
  if (!IsPlaying())
    return;

  auto& state = Core::System::GetInstance().GetAudioInterfaceState().GetData();

  const u32 old_sample_counter = state.sample_counter + 1;
  state.sample_counter += amount;

  if ((state.interrupt_timing - old_sample_counter) <= (state.sample_counter - old_sample_counter))
  {
    DEBUG_LOG_FMT(
        AUDIO_INTERFACE, "GenerateAudioInterrupt {:08x}:{:08x} at PC {:08x} control.AIINTVLD={}",
        state.sample_counter, state.interrupt_timing, PowerPC::ppcState.pc, state.control.AIINTVLD);
    GenerateAudioInterrupt();
  }
}

int GetAIPeriod()
{
  auto& state = Core::System::GetInstance().GetAudioInterfaceState().GetData();

  u64 period = state.cpu_cycles_per_sample * (state.interrupt_timing - state.sample_counter);
  u64 s_period = state.cpu_cycles_per_sample * Mixer::FIXED_SAMPLE_RATE_DIVIDEND /
                 state.ais_sample_rate_divisor;
  if (period == 0)
    return static_cast<int>(s_period);
  return static_cast<int>(std::min(period, s_period));
}

static void Update(Core::System& system, u64 userdata, s64 cycles_late)
{
  if (!IsPlaying())
    return;

  auto& state = system.GetAudioInterfaceState().GetData();
  auto& core_timing = system.GetCoreTiming();

  const u64 diff = core_timing.GetTicks() - state.last_cpu_time;
  if (diff > state.cpu_cycles_per_sample)
  {
    const u32 samples = static_cast<u32>(diff / state.cpu_cycles_per_sample);
    state.last_cpu_time += samples * state.cpu_cycles_per_sample;
    IncreaseSampleCount(samples);
  }
  core_timing.ScheduleEvent(GetAIPeriod() - cycles_late, state.event_type_ai);
}

void SetAIDSampleRate(SampleRate sample_rate)
{
  auto& state = Core::System::GetInstance().GetAudioInterfaceState().GetData();

  if (sample_rate == SampleRate::AI32KHz)
  {
    state.control.AIDFR = AID_32KHz;
    state.aid_sample_rate_divisor = Get32KHzSampleRateDivisor();
  }
  else
  {
    state.control.AIDFR = AID_48KHz;
    state.aid_sample_rate_divisor = Get48KHzSampleRateDivisor();
  }

  SoundStream* sound_stream = Core::System::GetInstance().GetSoundStream();
  sound_stream->GetMixer()->SetDMAInputSampleRateDivisor(state.aid_sample_rate_divisor);
}

void SetAISSampleRate(SampleRate sample_rate)
{
  auto& state = Core::System::GetInstance().GetAudioInterfaceState().GetData();

  if (sample_rate == SampleRate::AI32KHz)
  {
    state.control.AISFR = AIS_32KHz;
    state.ais_sample_rate_divisor = Get32KHzSampleRateDivisor();
  }
  else
  {
    state.control.AISFR = AIS_48KHz;
    state.ais_sample_rate_divisor = Get48KHzSampleRateDivisor();
  }

  state.cpu_cycles_per_sample = static_cast<u64>(SystemTimers::GetTicksPerSecond()) *
                                state.ais_sample_rate_divisor / Mixer::FIXED_SAMPLE_RATE_DIVIDEND;
  SoundStream* sound_stream = Core::System::GetInstance().GetSoundStream();
  sound_stream->GetMixer()->SetStreamInputSampleRateDivisor(state.ais_sample_rate_divisor);
}
}  // namespace

void Init()
{
  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  auto& state = system.GetAudioInterfaceState().GetData();

  state.control.hex = 0;
  SetAISSampleRate(SampleRate::AI48KHz);
  SetAIDSampleRate(SampleRate::AI32KHz);
  state.volume.hex = 0;
  state.sample_counter = 0;
  state.interrupt_timing = 0;

  state.last_cpu_time = 0;

  state.event_type_ai = core_timing.RegisterEvent("AICallback", Update);
}

void Shutdown()
{
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  auto& state = Core::System::GetInstance().GetAudioInterfaceState().GetData();

  mmio->Register(
      base | AI_CONTROL_REGISTER, MMIO::DirectRead<u32>(&state.control.hex),
      MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
        const AICR tmp_ai_ctrl(val);

        auto& core_timing = system.GetCoreTiming();
        auto& state = system.GetAudioInterfaceState().GetData();
        if (state.control.AIINTMSK != tmp_ai_ctrl.AIINTMSK)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Change AIINTMSK to {}", tmp_ai_ctrl.AIINTMSK);
          state.control.AIINTMSK = tmp_ai_ctrl.AIINTMSK;
        }

        if (state.control.AIINTVLD != tmp_ai_ctrl.AIINTVLD)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Change AIINTVLD to {}", tmp_ai_ctrl.AIINTVLD);
          state.control.AIINTVLD = tmp_ai_ctrl.AIINTVLD;
        }

        // Set frequency of streaming audio
        if (tmp_ai_ctrl.AISFR != state.control.AISFR)
        {
          // AISFR rates below are intentionally inverted wrt yagcd
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Change AISFR to {}",
                        tmp_ai_ctrl.AISFR ? "48khz" : "32khz");
          SetAISSampleRate(tmp_ai_ctrl.AISFR ? SampleRate::AI48KHz : SampleRate::AI32KHz);
        }

        // Set frequency of DMA
        if (tmp_ai_ctrl.AIDFR != state.control.AIDFR)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Change AIDFR to {}",
                        tmp_ai_ctrl.AIDFR ? "32khz" : "48khz");
          SetAIDSampleRate(tmp_ai_ctrl.AIDFR ? SampleRate::AI32KHz : SampleRate::AI48KHz);
        }

        // Streaming counter
        if (tmp_ai_ctrl.PSTAT != state.control.PSTAT)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "{} streaming audio",
                        tmp_ai_ctrl.PSTAT ? "start" : "stop");
          state.control.PSTAT = tmp_ai_ctrl.PSTAT;
          state.last_cpu_time = core_timing.GetTicks();

          core_timing.RemoveEvent(state.event_type_ai);
          core_timing.ScheduleEvent(GetAIPeriod(), state.event_type_ai);
        }

        // AI Interrupt
        if (tmp_ai_ctrl.AIINT)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Clear AIS Interrupt");
          state.control.AIINT = 0;
        }

        // Sample Count Reset
        if (tmp_ai_ctrl.SCRESET)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Reset AIS sample counter");
          state.sample_counter = 0;

          state.last_cpu_time = core_timing.GetTicks();
        }

        UpdateInterrupts();
      }));

  mmio->Register(base | AI_VOLUME_REGISTER, MMIO::DirectRead<u32>(&state.volume.hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& state = system.GetAudioInterfaceState().GetData();
                   state.volume.hex = val;
                   SoundStream* sound_stream = system.GetSoundStream();
                   sound_stream->GetMixer()->SetStreamingVolume(state.volume.left,
                                                                state.volume.right);
                 }));

  mmio->Register(base | AI_SAMPLE_COUNTER, MMIO::ComplexRead<u32>([](Core::System& system, u32) {
                   auto& state = system.GetAudioInterfaceState().GetData();
                   const u64 cycles_streamed =
                       IsPlaying() ? (system.GetCoreTiming().GetTicks() - state.last_cpu_time) :
                                     state.last_cpu_time;
                   return state.sample_counter +
                          static_cast<u32>(cycles_streamed / state.cpu_cycles_per_sample);
                 }),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& core_timing = system.GetCoreTiming();
                   auto& state = system.GetAudioInterfaceState().GetData();
                   state.sample_counter = val;
                   state.last_cpu_time = core_timing.GetTicks();
                   core_timing.RemoveEvent(state.event_type_ai);
                   core_timing.ScheduleEvent(GetAIPeriod(), state.event_type_ai);
                 }));

  mmio->Register(base | AI_INTERRUPT_TIMING, MMIO::DirectRead<u32>(&state.interrupt_timing),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& core_timing = system.GetCoreTiming();
                   auto& state = system.GetAudioInterfaceState().GetData();
                   DEBUG_LOG_FMT(AUDIO_INTERFACE, "AI_INTERRUPT_TIMING={:08x} at PC: {:08x}", val,
                                 PowerPC::ppcState.pc);
                   state.interrupt_timing = val;
                   core_timing.RemoveEvent(state.event_type_ai);
                   core_timing.ScheduleEvent(GetAIPeriod(), state.event_type_ai);
                 }));
}

void GenerateAISInterrupt()
{
  GenerateAudioInterrupt();
}

bool IsPlaying()
{
  auto& state = Core::System::GetInstance().GetAudioInterfaceState().GetData();
  return (state.control.PSTAT == 1);
}

u32 GetAIDSampleRateDivisor()
{
  auto& state = Core::System::GetInstance().GetAudioInterfaceState().GetData();
  return state.aid_sample_rate_divisor;
}

u32 GetAISSampleRateDivisor()
{
  auto& state = Core::System::GetInstance().GetAudioInterfaceState().GetData();
  return state.ais_sample_rate_divisor;
}

u32 Get32KHzSampleRateDivisor()
{
  return Get48KHzSampleRateDivisor() * 3 / 2;
}

u32 Get48KHzSampleRateDivisor()
{
  return (SConfig::GetInstance().bWii ? 1125 : 1124) * 2;
}
}  // namespace AudioInterface
