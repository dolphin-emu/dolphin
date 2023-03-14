// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// See CPP file for comments.

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;
namespace Core
{
class System;
}
namespace CoreTiming
{
struct EventType;
}
namespace MMIO
{
class Mapping;
}

namespace AudioInterface
{
enum class SampleRate
{
  AI32KHz,
  AI48KHz,
};

class AudioInterfaceManager
{
public:
  AudioInterfaceManager(Core::System& system);
  AudioInterfaceManager(const AudioInterfaceManager&) = delete;
  AudioInterfaceManager(AudioInterfaceManager&&) = delete;
  AudioInterfaceManager& operator=(const AudioInterfaceManager&) = delete;
  AudioInterfaceManager& operator=(AudioInterfaceManager&&) = delete;
  ~AudioInterfaceManager();

  void Init();
  void Shutdown();
  void DoState(PointerWrap& p);
  bool IsPlaying() const;

  void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

  // Get the audio rate divisors (divisors for 48KHz or 32KHz only)
  // Mixer::FIXED_SAMPLE_RATE_DIVIDEND will be the dividend used for these divisors
  u32 GetAIDSampleRateDivisor() const;
  u32 GetAISSampleRateDivisor() const;

  // The configured sample rate based on the control registers. Note that on GameCube, the named
  // rates are slightly higher than the names would suggest due to a hardware bug.
  SampleRate GetAIDSampleRate() const;
  SampleRate GetAISSampleRate() const;

  u32 Get32KHzSampleRateDivisor() const;
  u32 Get48KHzSampleRateDivisor() const;

  void GenerateAISInterrupt();

private:
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

  void UpdateInterrupts();
  void GenerateAudioInterrupt();
  void IncreaseSampleCount(const u32 amount);
  int GetAIPeriod() const;
  void SetAIDSampleRate(SampleRate sample_rate);
  void SetAISSampleRate(SampleRate sample_rate);

  void Update(u64 userdata, s64 cycles_late);
  static void GlobalUpdate(Core::System& system, u64 userdata, s64 cycles_late);

  // Registers
  AICR m_control;
  AIVR m_volume;

  u32 m_sample_counter = 0;
  u32 m_interrupt_timing = 0;

  u64 m_last_cpu_time = 0;
  u64 m_cpu_cycles_per_sample = 0;

  u32 m_ais_sample_rate_divisor = 0;
  u32 m_aid_sample_rate_divisor = 0;

  CoreTiming::EventType* m_event_type_ai = nullptr;

  Core::System& m_system;
};
}  // namespace AudioInterface
