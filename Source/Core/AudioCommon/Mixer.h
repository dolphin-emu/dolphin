// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <optional>

#include "AudioCommon/AudioStretcher.h"
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
  u32 Mix(s16* samples, u32 num_samples);
  u32 MixSurround(float* samples, u32 num_samples);

  // Called from main thread
  void PushSamples(const s16* samples, u32 num_samples);
  void PushStreamingSamples(const s16* samples, u32 num_samples);
  void PushWiimoteSpeakerSamples(const s16* samples, u32 num_samples, u32 sample_rate_divisor);
  void PushSkylanderPortalSamples(const u8* samples, u32 num_samples);
  void PushGBASamples(int device_number, const s16* samples, u32 num_samples);

  u32 GetSampleRate() const { return m_sampleRate; }

  void SetDMAInputSampleRateDivisor(u32 rate_divisor);
  void SetStreamInputSampleRateDivisor(u32 rate_divisor);
  void SetGBAInputSampleRateDivisors(int device_number, u32 rate_divisor);

  void SetStreamingVolume(u32 lvolume, u32 rvolume);
  void SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume);
  void SetGBAVolume(int device_number, u32 lvolume, u32 rvolume);

  void StartLogDTKAudio(const std::string& filename);
  void StopLogDTKAudio();

  void StartLogDSPAudio(const std::string& filename);
  void StopLogDSPAudio();

  // 54000000 doesn't work here as it doesn't evenly divide with 32000, but 108000000 does
  static constexpr u64 FIXED_SAMPLE_RATE_DIVIDEND = 54000000 * 2;

private:
  static constexpr u32 MAX_SAMPLES = 1024 * 4;  // 128 ms
  static constexpr u32 INDEX_MASK = MAX_SAMPLES - 1;
  static constexpr double MAX_PITCH_SHIFT = 1.0145453;  // 2 ^ (+1/48) - 1/4th Note Up
  static constexpr double MIN_PITCH_SHIFT = 0.9856632;  // 2 ^ (-1/48) - 1/4th Note Down
  static constexpr double SAMPLE_RATE_LPF = 1.0 / 4.0;  // How much to smooth out sample rate
  static constexpr double CONTROL_EFFORT = 1.0 / 64.0;  // Lowers the strength of pitch shifting

  const u32 SURROUND_CHANNELS = 6;

  class MixerFifo final
  {
  public:
    MixerFifo(Mixer* mixer, u32 sample_rate_divisor, bool little_endian)
        : m_mixer(mixer), m_input_sample_rate_divisor(sample_rate_divisor),
          m_little_endian(little_endian)
    {
    }
    void DoState(PointerWrap& p);
    void PushSamples(const s16* samples, u32 num_samples);
    std::pair<float, float> GetSample(u32 index, float frac, float sinc_ratio);
    void Mix(s16* samples, u32 num_samples, double target_speed);
    u32 MixRaw(s16* samples, u32 num_samples);
    void SetInputSampleRateDivisor(u64 rate_divisor);
    u64 GetInputSampleRateDivisor() const;
    void SetVolume(s32 lvolume, s32 rvolume);
    std::pair<s32, s32> GetVolume() const;
    u32 AvailableFIFOSamples() const;
    u32 AvailableSamples() const;
    DT AvailableSamplesTime() const;

  private:
    Mixer* m_mixer;
    u64 m_input_sample_rate_divisor;
    bool m_little_endian;
    std::array<std::pair<float, float>, MAX_SAMPLES> m_buffer{};
    std::atomic<u32> m_indexW{0};
    std::atomic<u32> m_indexR{0};
    // Volume ranges from 0-256
    std::atomic<s32> m_LVolume{256};
    std::atomic<s32> m_RVolume{256};

    double m_multiplier = 1.0;
    double m_frac = 0.0;
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
  u32 m_sampleRate;

  bool m_is_stretching = false;
  AudioCommon::AudioStretcher m_stretcher;
  AudioCommon::SurroundDecoder m_surround_decoder;
  std::array<s16, MAX_SAMPLES * 2> m_scratch_buffer{};

  WaveFileWriter m_wave_writer_dtk;
  WaveFileWriter m_wave_writer_dsp;

  bool m_log_dtk_audio = false;
  bool m_log_dsp_audio = false;

  float m_config_emulation_speed;
  bool m_config_audio_stretch;
  int m_config_direct_latency;
  int m_config_sinc_window_width;

  Config::ConfigChangedCallbackID m_config_changed_callback_id;
};
