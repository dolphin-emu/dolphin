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

AudioInterfaceManager::AudioInterfaceManager(Core::System& system)
    : m_ais_sample_rate_divisor(Mixer::FIXED_SAMPLE_RATE_DIVIDEND / 48000),
      m_aid_sample_rate_divisor(Mixer::FIXED_SAMPLE_RATE_DIVIDEND / 32000), m_system(system)
{
}

AudioInterfaceManager::~AudioInterfaceManager() = default;

void AudioInterfaceManager::DoState(PointerWrap& p)
{
  p.Do(m_control);
  p.Do(m_volume);
  p.Do(m_sample_counter);
  p.Do(m_interrupt_timing);
  p.Do(m_last_cpu_time);
  p.Do(m_ais_sample_rate_divisor);
  p.Do(m_aid_sample_rate_divisor);
  p.Do(m_cpu_cycles_per_sample);

  SoundStream* sound_stream = m_system.GetSoundStream();
  sound_stream->GetMixer()->DoState(p);
}

void AudioInterfaceManager::UpdateInterrupts()
{
  m_system.GetProcessorInterface().SetInterrupt(ProcessorInterface::INT_CAUSE_AI,
                                                m_control.AIINT & m_control.AIINTMSK);
}

void AudioInterfaceManager::GenerateAudioInterrupt()
{
  m_control.AIINT = 1;
  UpdateInterrupts();
}

void AudioInterfaceManager::IncreaseSampleCount(const u32 amount)
{
  if (!IsPlaying())
    return;

  const u32 old_sample_counter = m_sample_counter + 1;
  m_sample_counter += amount;

  if ((m_interrupt_timing - old_sample_counter) <= (m_sample_counter - old_sample_counter))
  {
    DEBUG_LOG_FMT(
        AUDIO_INTERFACE, "GenerateAudioInterrupt {:08x}:{:08x} at PC {:08x} control.AIINTVLD={}",
        m_sample_counter, m_interrupt_timing, m_system.GetPPCState().pc, m_control.AIINTVLD);
    GenerateAudioInterrupt();
  }
}

int AudioInterfaceManager::GetAIPeriod() const
{
  u64 period = m_cpu_cycles_per_sample * (m_interrupt_timing - m_sample_counter);
  u64 s_period =
      m_cpu_cycles_per_sample * Mixer::FIXED_SAMPLE_RATE_DIVIDEND / m_ais_sample_rate_divisor;
  if (period == 0)
    return static_cast<int>(s_period);
  return static_cast<int>(std::min(period, s_period));
}

void AudioInterfaceManager::GlobalUpdate(Core::System& system, u64 userdata, s64 cycles_late)
{
  system.GetAudioInterface().Update(userdata, cycles_late);
}

void AudioInterfaceManager::Update(u64 userdata, s64 cycles_late)
{
  if (!IsPlaying())
    return;

  auto& core_timing = m_system.GetCoreTiming();

  const u64 diff = core_timing.GetTicks() - m_last_cpu_time;
  if (diff > m_cpu_cycles_per_sample)
  {
    const u32 samples = static_cast<u32>(diff / m_cpu_cycles_per_sample);
    m_last_cpu_time += samples * m_cpu_cycles_per_sample;
    IncreaseSampleCount(samples);
  }
  core_timing.ScheduleEvent(GetAIPeriod() - cycles_late, m_event_type_ai);
}

void AudioInterfaceManager::SetAIDSampleRate(SampleRate sample_rate)
{
  if (sample_rate == SampleRate::AI32KHz)
  {
    m_control.AIDFR = AID_32KHz;
    m_aid_sample_rate_divisor = Get32KHzSampleRateDivisor();
  }
  else
  {
    m_control.AIDFR = AID_48KHz;
    m_aid_sample_rate_divisor = Get48KHzSampleRateDivisor();
  }

  SoundStream* sound_stream = m_system.GetSoundStream();
  sound_stream->GetMixer()->SetDMAInputSampleRateDivisor(m_aid_sample_rate_divisor);
}

void AudioInterfaceManager::SetAISSampleRate(SampleRate sample_rate)
{
  if (sample_rate == SampleRate::AI32KHz)
  {
    m_control.AISFR = AIS_32KHz;
    m_ais_sample_rate_divisor = Get32KHzSampleRateDivisor();
  }
  else
  {
    m_control.AISFR = AIS_48KHz;
    m_ais_sample_rate_divisor = Get48KHzSampleRateDivisor();
  }

  m_cpu_cycles_per_sample = static_cast<u64>(m_system.GetSystemTimers().GetTicksPerSecond()) *
                            m_ais_sample_rate_divisor / Mixer::FIXED_SAMPLE_RATE_DIVIDEND;
  SoundStream* sound_stream = m_system.GetSoundStream();
  sound_stream->GetMixer()->SetStreamInputSampleRateDivisor(m_ais_sample_rate_divisor);
}

void AudioInterfaceManager::Init()
{
  m_control.hex = 0;
  SetAISSampleRate(SampleRate::AI48KHz);
  SetAIDSampleRate(SampleRate::AI32KHz);
  m_volume.hex = 0;
  m_sample_counter = 0;
  m_interrupt_timing = 0;

  m_last_cpu_time = 0;

  m_event_type_ai = m_system.GetCoreTiming().RegisterEvent("AICallback", GlobalUpdate);
}

void AudioInterfaceManager::Shutdown()
{
}

void AudioInterfaceManager::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  mmio->Register(
      base | AI_CONTROL_REGISTER, MMIO::DirectRead<u32>(&m_control.hex),
      MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
        const AICR tmp_ai_ctrl(val);

        auto& core_timing = system.GetCoreTiming();
        auto& ai = system.GetAudioInterface();
        if (ai.m_control.AIINTMSK != tmp_ai_ctrl.AIINTMSK)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Change AIINTMSK to {}", tmp_ai_ctrl.AIINTMSK);
          ai.m_control.AIINTMSK = tmp_ai_ctrl.AIINTMSK;
        }

        if (ai.m_control.AIINTVLD != tmp_ai_ctrl.AIINTVLD)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Change AIINTVLD to {}", tmp_ai_ctrl.AIINTVLD);
          ai.m_control.AIINTVLD = tmp_ai_ctrl.AIINTVLD;
        }

        // Set frequency of streaming audio
        if (tmp_ai_ctrl.AISFR != ai.m_control.AISFR)
        {
          // AISFR rates below are intentionally inverted wrt yagcd
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Change AISFR to {}",
                        tmp_ai_ctrl.AISFR ? "48khz" : "32khz");
          ai.SetAISSampleRate(tmp_ai_ctrl.AISFR ? SampleRate::AI48KHz : SampleRate::AI32KHz);
        }

        // Set frequency of DMA
        if (tmp_ai_ctrl.AIDFR != ai.m_control.AIDFR)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Change AIDFR to {}",
                        tmp_ai_ctrl.AIDFR ? "32khz" : "48khz");
          ai.SetAIDSampleRate(tmp_ai_ctrl.AIDFR ? SampleRate::AI32KHz : SampleRate::AI48KHz);
        }

        // Streaming counter
        if (tmp_ai_ctrl.PSTAT != ai.m_control.PSTAT)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "{} streaming audio",
                        tmp_ai_ctrl.PSTAT ? "start" : "stop");
          ai.m_control.PSTAT = tmp_ai_ctrl.PSTAT;
          ai.m_last_cpu_time = core_timing.GetTicks();

          core_timing.RemoveEvent(ai.m_event_type_ai);
          core_timing.ScheduleEvent(ai.GetAIPeriod(), ai.m_event_type_ai);
        }

        // AI Interrupt
        if (tmp_ai_ctrl.AIINT)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Clear AIS Interrupt");
          ai.m_control.AIINT = 0;
        }

        // Sample Count Reset
        if (tmp_ai_ctrl.SCRESET)
        {
          DEBUG_LOG_FMT(AUDIO_INTERFACE, "Reset AIS sample counter");
          ai.m_sample_counter = 0;

          ai.m_last_cpu_time = core_timing.GetTicks();
        }

        ai.UpdateInterrupts();
      }));

  mmio->Register(base | AI_VOLUME_REGISTER, MMIO::DirectRead<u32>(&m_volume.hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& ai = system.GetAudioInterface();
                   ai.m_volume.hex = val;
                   SoundStream* sound_stream = system.GetSoundStream();
                   sound_stream->GetMixer()->SetStreamingVolume(ai.m_volume.left,
                                                                ai.m_volume.right);
                 }));

  mmio->Register(base | AI_SAMPLE_COUNTER, MMIO::ComplexRead<u32>([](Core::System& system, u32) {
                   auto& ai = system.GetAudioInterface();
                   const u64 cycles_streamed =
                       ai.IsPlaying() ? (system.GetCoreTiming().GetTicks() - ai.m_last_cpu_time) :
                                        ai.m_last_cpu_time;
                   return ai.m_sample_counter +
                          static_cast<u32>(cycles_streamed / ai.m_cpu_cycles_per_sample);
                 }),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& core_timing = system.GetCoreTiming();
                   auto& ai = system.GetAudioInterface();
                   ai.m_sample_counter = val;
                   ai.m_last_cpu_time = core_timing.GetTicks();
                   core_timing.RemoveEvent(ai.m_event_type_ai);
                   core_timing.ScheduleEvent(ai.GetAIPeriod(), ai.m_event_type_ai);
                 }));

  mmio->Register(base | AI_INTERRUPT_TIMING, MMIO::DirectRead<u32>(&m_interrupt_timing),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& core_timing = system.GetCoreTiming();
                   auto& ai = system.GetAudioInterface();
                   DEBUG_LOG_FMT(AUDIO_INTERFACE, "AI_INTERRUPT_TIMING={:08x} at PC: {:08x}", val,
                                 system.GetPPCState().pc);
                   ai.m_interrupt_timing = val;
                   core_timing.RemoveEvent(ai.m_event_type_ai);
                   core_timing.ScheduleEvent(ai.GetAIPeriod(), ai.m_event_type_ai);
                 }));
}

void AudioInterfaceManager::GenerateAISInterrupt()
{
  GenerateAudioInterrupt();
}

bool AudioInterfaceManager::IsPlaying() const
{
  return (m_control.PSTAT == 1);
}

u32 AudioInterfaceManager::GetAIDSampleRateDivisor() const
{
  return m_aid_sample_rate_divisor;
}

u32 AudioInterfaceManager::GetAISSampleRateDivisor() const
{
  return m_ais_sample_rate_divisor;
}

SampleRate AudioInterfaceManager::GetAIDSampleRate() const
{
  return m_control.AIDFR == AID_48KHz ? SampleRate::AI48KHz : SampleRate::AI32KHz;
}

SampleRate AudioInterfaceManager::GetAISSampleRate() const
{
  return m_control.AISFR == AIS_32KHz ? SampleRate::AI32KHz : SampleRate::AI48KHz;
}

u32 AudioInterfaceManager::Get32KHzSampleRateDivisor() const
{
  return Get48KHzSampleRateDivisor() * 3 / 2;
}

u32 AudioInterfaceManager::Get48KHzSampleRateDivisor() const
{
  return (m_system.IsWii() ? 1125 : 1124) * 2;
}
}  // namespace AudioInterface
