// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>

#include "AudioCommon/SurroundDecoder.h"
#include "AudioCommon/WaveFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

class PointerWrap;

class Mixer final
{
public:
  explicit Mixer(u32 BackendSampleRate);
  ~Mixer();

  void DoState(PointerWrap& p);

  // Called from audio threads
  std::size_t Mix(s16* samples, std::size_t numSamples);
  std::size_t MixSurround(float* samples, std::size_t num_samples);

  // Called from main thread
  void PushSamples(const s16* samples, std::size_t num_samples);
  void PushStreamingSamples(const s16* samples, std::size_t num_samples);
  void PushWiimoteSpeakerSamples(const s16* samples, std::size_t num_samples,
                                 u32 sample_rate_divisor);
  void PushSkylanderPortalSamples(const u8* samples, std::size_t num_samples);
  void PushGBASamples(std::size_t device_number, const s16* samples, std::size_t num_samples);

  u32 GetSampleRate() const { return m_output_sample_rate; }

  void SetDMAInputSampleRateDivisor(u32 rate_divisor);
  void SetStreamInputSampleRateDivisor(u32 rate_divisor);
  void SetGBAInputSampleRateDivisors(std::size_t device_number, u32 rate_divisor);

  void SetStreamingVolume(u32 lvolume, u32 rvolume);
  void SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume);
  void SetGBAVolume(std::size_t device_number, u32 lvolume, u32 rvolume);

  void StartLogDTKAudio(const std::string& filename);
  void StopLogDTKAudio();

  void StartLogDSPAudio(const std::string& filename);
  void StopLogDSPAudio();

  // 54000000 doesn't work here as it doesn't evenly divide with 32000, but 108000000 does
  static constexpr u64 FIXED_SAMPLE_RATE_DIVIDEND = 54000000 * 2;

private:
  const std::size_t SURROUND_CHANNELS = 6;

  class MixerFifo final
  {
    static constexpr std::size_t GRANULE_QUEUE_SIZE = 20;

    template <typename T>
    static s16 ToShort(const T x)
    {
      return static_cast<s16>(std::clamp<T>(x, static_cast<T>(std::numeric_limits<s16>::min()),
                                            static_cast<T>(std::numeric_limits<s16>::max())));
    }
    struct StereoPair final
    {
      float l = 0.f;
      float r = 0.f;

      constexpr StereoPair() = default;
      constexpr explicit StereoPair(float mono) : l(mono), r(mono) {}
      constexpr StereoPair(float left, float right) : l(left), r(right) {}
      constexpr StereoPair(s16 left, s16 right) : l(left), r(right) {}

      StereoPair operator+(const StereoPair& other) const
      {
        return StereoPair(l + other.l, r + other.r);
      }

      StereoPair& operator*=(const StereoPair& other)
      {
        l *= other.l;
        r *= other.r;
        return *this;
      }
    };

    static constexpr std::size_t GRANULE_BUFFER_SIZE = 256;
    static constexpr std::size_t GRANULE_BUFFER_MASK = GRANULE_BUFFER_SIZE - 1;
    static constexpr std::size_t GRANULE_BUFFER_BITS = std::countr_one(GRANULE_BUFFER_MASK);
    static constexpr std::size_t GRANULE_BUFFER_FRAC_BITS = 32 - GRANULE_BUFFER_BITS;

    using GranuleBuffer = std::array<StereoPair, GRANULE_BUFFER_SIZE>;
    class Granule final
    {
    public:
      constexpr Granule() = default;
      constexpr Granule(const GranuleBuffer& input, std::size_t start_index);

      static StereoPair InterpStereoPair(const Granule& front, const Granule& back, u32 frac);

      Granule& operator*=(const StereoPair& x)
      {
        for (auto& sample : m_buffer)
          sample *= x;
        return *this;
      }

    private:
      GranuleBuffer m_buffer{};
    };

  public:
    MixerFifo(Mixer* mixer, u32 sample_rate_divisor, bool little_endian)
        : m_mixer(mixer), m_input_sample_rate_divisor(sample_rate_divisor),
          m_little_endian(little_endian)
    {
    }
    void DoState(PointerWrap& p);
    void PushSamples(const s16* samples, std::size_t num_samples);
    void Mix(s16* samples, std::size_t num_samples);
    void SetInputSampleRateDivisor(u32 rate_divisor);
    u32 GetInputSampleRateDivisor() const;
    void SetVolume(u32 lvolume, u32 rvolume);
    std::pair<s32, s32> GetVolume() const;

  private:
    Mixer* m_mixer;
    u32 m_input_sample_rate_divisor;
    bool m_little_endian;

    std::size_t m_buffer_index = 0;
    GranuleBuffer m_buffer{};

    u32 m_current_index = 0;
    Granule m_front, m_back;

    std::array<Granule, GRANULE_QUEUE_SIZE> m_queue;
    std::atomic<std::size_t> m_queue_head{0};
    std::atomic<std::size_t> m_queue_tail{0};
    std::atomic<bool> m_queue_looping{false};
    std::size_t m_queue_fade_index = 0;

    void Enqueue(const Granule& granule);
    void Dequeue(Granule* granule);

    // Volume ranges from 0-256
    std::atomic<s32> m_LVolume{256};
    std::atomic<s32> m_RVolume{256};

    StereoPair m_quantization_error;
  };

  void RefreshConfig();

  MixerFifo m_dma_mixer{this, FIXED_SAMPLE_RATE_DIVIDEND / 32000, false};
  MixerFifo m_streaming_mixer{this, FIXED_SAMPLE_RATE_DIVIDEND / 48000, false};
  MixerFifo m_wiimote_speaker_mixer{this, FIXED_SAMPLE_RATE_DIVIDEND / 3000, true};
  MixerFifo m_skylander_portal_mixer{this, FIXED_SAMPLE_RATE_DIVIDEND / 8000, true};
  std::array<MixerFifo, 4> m_gba_mixers{MixerFifo{this, FIXED_SAMPLE_RATE_DIVIDEND / 48000, true},
                                        MixerFifo{this, FIXED_SAMPLE_RATE_DIVIDEND / 48000, true},
                                        MixerFifo{this, FIXED_SAMPLE_RATE_DIVIDEND / 48000, true},
                                        MixerFifo{this, FIXED_SAMPLE_RATE_DIVIDEND / 48000, true}};
  u32 m_output_sample_rate;

  AudioCommon::SurroundDecoder m_surround_decoder;

  WaveFileWriter m_wave_writer_dtk;
  WaveFileWriter m_wave_writer_dsp;

  bool m_log_dtk_audio = false;
  bool m_log_dsp_audio = false;

  float m_config_emulation_speed;
  bool m_audio_fill_gaps = true;

  Config::ConfigChangedCallbackID m_config_changed_callback_id;
};
