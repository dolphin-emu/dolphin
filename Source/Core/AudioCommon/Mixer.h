// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>

#include "AudioCommon/AudioStretcher.h"
#include "AudioCommon/SurroundDecoder.h"
#include "AudioCommon/WaveFile.h"
#include "Common/CommonTypes.h"

class PointerWrap;

class Mixer final
{
public:
  explicit Mixer(unsigned int BackendSampleRate);
  ~Mixer();

  void DoState(PointerWrap& p);

  // Called from audio threads
  unsigned int Mix(short* samples, unsigned int numSamples);
  unsigned int MixSurround(float* samples, unsigned int num_samples);

  // Called from main thread
  void PushSamples(const short* samples, unsigned int num_samples);
  void PushStreamingSamples(const short* samples, unsigned int num_samples);
  void PushWiimoteSpeakerSamples(const short* samples, unsigned int num_samples,
                                 unsigned int sample_rate_divisor);
  void PushSkylanderPortalSamples(const u8* samples, unsigned int num_samples);
  void PushGBASamples(int device_number, const short* samples, unsigned int num_samples);

  unsigned int GetSampleRate() const { return m_sampleRate; }

  void SetDMAInputSampleRateDivisor(unsigned int rate_divisor);
  void SetStreamInputSampleRateDivisor(unsigned int rate_divisor);
  void SetGBAInputSampleRateDivisors(int device_number, unsigned int rate_divisor);

  void SetStreamingVolume(unsigned int lvolume, unsigned int rvolume);
  void SetWiimoteSpeakerVolume(unsigned int lvolume, unsigned int rvolume);
  void SetGBAVolume(int device_number, unsigned int lvolume, unsigned int rvolume);

  void StartLogDTKAudio(const std::string& filename);
  void StopLogDTKAudio();

  void StartLogDSPAudio(const std::string& filename);
  void StopLogDSPAudio();

  // 54000000 doesn't work here as it doesn't evenly divide with 32000, but 108000000 does
  static constexpr u64 FIXED_SAMPLE_RATE_DIVIDEND = 54000000 * 2;

private:
  static constexpr u32 MAX_SAMPLES = 1024 * 4;  // 128 ms
  static constexpr u32 INDEX_MASK = MAX_SAMPLES * 2 - 1;
  static constexpr int MAX_FREQ_SHIFT = 200;  // Per 32000 Hz
  static constexpr float CONTROL_FACTOR = 0.2f;
  static constexpr u32 CONTROL_AVG = 32;  // In freq_shift per FIFO size offset

  const unsigned int SURROUND_CHANNELS = 6;

  class MixerFifo final
  {
  public:
    MixerFifo(Mixer* mixer, unsigned sample_rate_divisor, bool little_endian)
        : m_mixer(mixer), m_input_sample_rate_divisor(sample_rate_divisor),
          m_little_endian(little_endian)
    {
    }
    void DoState(PointerWrap& p);
    void PushSamples(const short* samples, unsigned int num_samples);
    unsigned int Mix(short* samples, unsigned int numSamples, bool consider_framelimit,
                     float emulationspeed, int timing_variance);
    void SetInputSampleRateDivisor(unsigned int rate_divisor);
    unsigned int GetInputSampleRateDivisor() const;
    void SetVolume(unsigned int lvolume, unsigned int rvolume);
    std::pair<s32, s32> GetVolume() const;
    unsigned int AvailableSamples() const;

  private:
    Mixer* m_mixer;
    unsigned m_input_sample_rate_divisor;
    bool m_little_endian;
    std::array<short, MAX_SAMPLES * 2> m_buffer{};
    std::atomic<u32> m_indexW{0};
    std::atomic<u32> m_indexR{0};
    // Volume ranges from 0-256
    std::atomic<s32> m_LVolume{256};
    std::atomic<s32> m_RVolume{256};
    float m_numLeftI = 0.0f;
    u32 m_frac = 0;
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
  unsigned int m_sampleRate;

  bool m_is_stretching = false;
  AudioCommon::AudioStretcher m_stretcher;
  AudioCommon::SurroundDecoder m_surround_decoder;
  std::array<short, MAX_SAMPLES * 2> m_scratch_buffer{};

  WaveFileWriter m_wave_writer_dtk;
  WaveFileWriter m_wave_writer_dsp;

  bool m_log_dtk_audio = false;
  bool m_log_dsp_audio = false;

  float m_config_emulation_speed;
  int m_config_timing_variance;
  bool m_config_audio_stretch;

  size_t m_config_changed_callback_id;
};
