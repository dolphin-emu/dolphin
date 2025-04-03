// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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
  void PushWiimoteSpeakerSamples(const s16* samples, std::size_t num_samples, u32 sample_rate);
  void PushSkylanderPortalSamples(const u8* samples, std::size_t num_samples);
  void PushGBASamples(std::size_t device_number, const s16* samples, std::size_t num_samples);

  u32 GetSampleRate() const { return m_output_sample_rate; }

  void SetDMAInputSampleRate(u32 sample_rate);
  void SetStreamInputSampleRate(u32 sample_rate);
  void SetGBAInputSampleRate(std::size_t device_number, u32 sample_rate);

  void SetStreamingVolume(u32 lvolume, u32 rvolume);
  void SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume);
  void SetGBAVolume(std::size_t device_number, u32 lvolume, u32 rvolume);

  void StartLogDTKAudio(const std::string& filename);
  void StopLogDTKAudio();

  void StartLogDSPAudio(const std::string& filename);
  void StopLogDSPAudio();

private:
  const std::size_t SURROUND_CHANNELS = 6;

  class MixerFifo final
  {
    static constexpr std::size_t MAX_GRANULE_QUEUE_SIZE = 256;
    static constexpr std::size_t GRANULE_QUEUE_MASK = MAX_GRANULE_QUEUE_SIZE - 1;

    struct StereoPair final
    {
      float l = 0.f;
      float r = 0.f;

      constexpr StereoPair() = default;
      constexpr StereoPair(const StereoPair&) = default;
      constexpr StereoPair& operator=(const StereoPair&) = default;
      constexpr StereoPair(StereoPair&&) = default;
      constexpr StereoPair& operator=(StereoPair&&) = default;

      constexpr StereoPair(float mono) : l(mono), r(mono) {}
      constexpr StereoPair(float left, float right) : l(left), r(right) {}
      constexpr StereoPair(s16 left, s16 right) : l(left), r(right) {}

      StereoPair operator+(const StereoPair& other) const
      {
        return StereoPair(l + other.l, r + other.r);
      }

      StereoPair operator*(const StereoPair& other) const
      {
        return StereoPair(l * other.l, r * other.r);
      }
    };

    static constexpr std::size_t GRANULE_SIZE = 256;
    static constexpr std::size_t GRANULE_OVERLAP = GRANULE_SIZE / 2;
    static constexpr std::size_t GRANULE_MASK = GRANULE_SIZE - 1;
    static constexpr std::size_t GRANULE_BITS = std::countr_one(GRANULE_MASK);
    static constexpr std::size_t GRANULE_FRAC_BITS = 32 - GRANULE_BITS;

    using Granule = std::array<StereoPair, GRANULE_SIZE>;

  public:
    MixerFifo(Mixer* mixer, u32 sample_rate, bool little_endian)
        : m_mixer(mixer), m_input_sample_rate(sample_rate), m_little_endian(little_endian)
    {
    }
    void DoState(PointerWrap& p);
    void PushSamples(const s16* samples, std::size_t num_samples);
    void Mix(s16* samples, std::size_t num_samples);
    void SetInputSampleRate(u32 sample_rate);
    u32 GetInputSampleRate() const;
    void SetVolume(u32 lvolume, u32 rvolume);
    std::pair<s32, s32> GetVolume() const;

  private:
    Mixer* m_mixer;
    u32 m_input_sample_rate;
    bool m_little_endian;

    Granule m_next_buffer{};
    std::size_t m_next_buffer_index = 0;

    u32 m_current_index = 0;
    Granule m_front, m_back;

    std::atomic<std::size_t> m_granule_queue_size{20};
    std::array<Granule, MAX_GRANULE_QUEUE_SIZE> m_queue;
    std::atomic<std::size_t> m_queue_head{0};
    std::atomic<std::size_t> m_queue_tail{0};
    std::atomic<bool> m_queue_looping{false};
    float m_fade_volume = 1.0;

    void Enqueue();
    void Dequeue(Granule* granule);

    // Volume ranges from 0-256
    std::atomic<s32> m_LVolume{256};
    std::atomic<s32> m_RVolume{256};

    StereoPair m_quantization_error;
  };

  void RefreshConfig();

  MixerFifo m_dma_mixer{this, 32000, false};
  MixerFifo m_streaming_mixer{this, 48000, false};
  MixerFifo m_wiimote_speaker_mixer{this, 3000, true};
  MixerFifo m_skylander_portal_mixer{this, 8000, true};
  std::array<MixerFifo, 4> m_gba_mixers{MixerFifo{this, 48000, true}, MixerFifo{this, 48000, true},
                                        MixerFifo{this, 48000, true}, MixerFifo{this, 48000, true}};
  u32 m_output_sample_rate;

  AudioCommon::SurroundDecoder m_surround_decoder;

  WaveFileWriter m_wave_writer_dtk;
  WaveFileWriter m_wave_writer_dsp;

  bool m_log_dtk_audio = false;
  bool m_log_dsp_audio = false;

  float m_config_emulation_speed;
  bool m_config_fill_audio_gaps;
  int m_config_audio_buffer_ms;

  Config::ConfigChangedCallbackID m_config_changed_callback_id;
};
