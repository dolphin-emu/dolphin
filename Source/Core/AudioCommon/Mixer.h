// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <memory>

#include "AudioCommon/BaseFilter.h"
#include "AudioCommon/WaveFile.h"
#include "Common/CommonTypes.h"
#include "Common/RingBuffer.h"

class Dither;

class CMixer final
{
public:
  explicit CMixer(u32 BackendSampleRate);
  ~CMixer();
  // Called from audio threads
  u32 Mix(s16* samples, u32 numSamples, bool consider_framelimit = true);

  // Called from main thread
  void PushSamples(const s16* samples, u32 num_samples);
  void PushStreamingSamples(const s16* samples, u32 num_samples);
  void PushWiimoteSpeakerSamples(u32 wiimote_index, const s16* samples, u32 num_samples,
                                 u32 sample_rate);
  u32 GetSampleRate() const { return m_output_sample_rate; }
  void SetDMAInputSampleRate(u32 rate);
  void SetStreamInputSampleRate(u32 rate);
  void SetStreamingVolume(u32 lvolume, u32 rvolume);
  void SetWiimoteSpeakerVolume(u32 wiimote_index, u32 lvolume, u32 rvolume);

  void StartLogDTKAudio(const std::string& filename);
  void StopLogDTKAudio();

  void StartLogDSPAudio(const std::string& filename);
  void StopLogDSPAudio();

  void StartLogMixAudio(const std::string& filename);
  void StopLogMixAudio();

  float GetCurrentSpeed() const { return m_speed.load(); }
  void UpdateSpeed(float val) { m_speed.store(val); }
private:
  static constexpr u32 MAX_SAMPLES = 1024 * 4;  // 128 ms
  static constexpr u32 INDEX_MASK = MAX_SAMPLES * 2 - 1;
  static constexpr int MAX_FREQ_SHIFT = 200;  // Per 32000 Hz
  static constexpr float CONTROL_FACTOR = 0.2f;
  static constexpr u32 CONTROL_AVG = 32;  // In freq_shift per FIFO size offset

  class MixerFifo
  {
  public:
    MixerFifo(CMixer* mixer, unsigned sample_rate, std::shared_ptr<BaseFilter> filter = nullptr);

    void PushSamples(const s16* samples, u32 num_samples);
    u32 Mix(std::array<float, MAX_SAMPLES * 2>& samples, u32 numSamples,
            bool consider_framelimit = true);
    void SetInputSampleRate(u32 rate);
    u32 GetInputSampleRate() const;
    void SetVolume(u32 lvolume, u32 rvolume);

  private:
    CMixer* m_mixer;
    std::shared_ptr<BaseFilter> m_filter;
    RingBuffer<float> m_floats;
    RingBuffer<s16> m_shorts;

    unsigned m_input_sample_rate;

    float m_numLeftI = 0.0f;
    float m_frac = 0;
    // Volume ranges from 0-256
    std::atomic<s32> m_LVolume{256};
    std::atomic<s32> m_RVolume{256};
  };

  std::array<float, MAX_SAMPLES * 2> m_accumulator{};

  std::unique_ptr<MixerFifo> m_dma_mixer;
  std::unique_ptr<MixerFifo> m_streaming_mixer;
  // max # wiimotes = 4, one mixer per wiimote
  std::array<std::unique_ptr<MixerFifo>, 4> m_wiimote_speaker_mixers;
  std::unique_ptr<Dither> m_dither;
  std::shared_ptr<BaseFilter> m_linear_filter;

  WaveFileWriter m_wave_writer_dtk;
  WaveFileWriter m_wave_writer_dsp;
  WaveFileWriter m_wave_writer_mix;

  bool m_log_dtk_audio = false;
  bool m_log_dsp_audio = false;
  bool m_log_mix_audio = false;

  u32 m_output_sample_rate;
  // Current rate of emulation (1.0 = 100% speed)
  std::atomic<float> m_speed{0.0f};
};