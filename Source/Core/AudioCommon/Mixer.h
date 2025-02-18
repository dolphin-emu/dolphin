// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>

#include "AudioCommon/SurroundDecoder.h"
#include "AudioCommon/WaveFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

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
  const unsigned int SURROUND_CHANNELS = 6;

  class MixerFifo final
  {
    static constexpr size_t GRANULE_QUEUE_SIZE = 20;

    /**
     * import numpy as np
     * import scipy.signal as signal
     * window = np.cumsum(signal.windows.kaiser(128 + 1, beta=8.6))
     * window = np.concatenate([window[:-1], window[::-1][1:]])
     * window = np.clip(window / window.max(), -1, 1)
     * window = np.floor(window * (2 ** 16 - 1))
     * elements = ", ".join([f"0x{int(x):04x}" for x in window])
     * print(f'static constexpr int GRANULE_WINDOW[{len(window)}] = {{ {elements} }};')
     */
    static constexpr int GRANULE_WINDOW[256] = {
        0x0001, 0x0004, 0x0008, 0x000e, 0x0016, 0x0020, 0x002d, 0x003e, 0x0053, 0x006d, 0x008d,
        0x00b2, 0x00df, 0x0114, 0x0151, 0x0198, 0x01ea, 0x0248, 0x02b3, 0x032d, 0x03b5, 0x044e,
        0x04f8, 0x05b6, 0x0688, 0x076f, 0x086d, 0x0982, 0x0ab1, 0x0bfa, 0x0d5f, 0x0edf, 0x107e,
        0x123a, 0x1416, 0x1611, 0x182e, 0x1a6b, 0x1ccb, 0x1f4c, 0x21ef, 0x24b5, 0x279e, 0x2aa8,
        0x2dd4, 0x3122, 0x3490, 0x381f, 0x3bcc, 0x3f98, 0x4380, 0x4784, 0x4ba3, 0x4fda, 0x5427,
        0x588a, 0x5d00, 0x6187, 0x661d, 0x6ac0, 0x6f6e, 0x7424, 0x78e0, 0x7d9f, 0x8260, 0x8720,
        0x8bdc, 0x9092, 0x953f, 0x99e2, 0x9e78, 0xa300, 0xa775, 0xabd8, 0xb026, 0xb45d, 0xb87b,
        0xbc7f, 0xc068, 0xc434, 0xc7e1, 0xcb70, 0xcede, 0xd22b, 0xd558, 0xd862, 0xdb4a, 0xde10,
        0xe0b4, 0xe335, 0xe594, 0xe7d2, 0xe9ee, 0xebea, 0xedc6, 0xef82, 0xf120, 0xf2a1, 0xf405,
        0xf54e, 0xf67d, 0xf793, 0xf891, 0xf978, 0xfa4a, 0xfb07, 0xfbb2, 0xfc4b, 0xfcd3, 0xfd4c,
        0xfdb7, 0xfe15, 0xfe67, 0xfeaf, 0xfeec, 0xff21, 0xff4d, 0xff73, 0xff92, 0xffac, 0xffc1,
        0xffd2, 0xffe0, 0xffea, 0xfff2, 0xfff8, 0xfffc, 0xffff, 0xffff, 0xfffc, 0xfff8, 0xfff2,
        0xffea, 0xffe0, 0xffd2, 0xffc1, 0xffac, 0xff92, 0xff73, 0xff4d, 0xff21, 0xfeec, 0xfeaf,
        0xfe67, 0xfe15, 0xfdb7, 0xfd4c, 0xfcd3, 0xfc4b, 0xfbb2, 0xfb07, 0xfa4a, 0xf978, 0xf891,
        0xf793, 0xf67d, 0xf54e, 0xf405, 0xf2a1, 0xf120, 0xef82, 0xedc6, 0xebea, 0xe9ee, 0xe7d2,
        0xe594, 0xe335, 0xe0b4, 0xde10, 0xdb4a, 0xd862, 0xd558, 0xd22b, 0xcede, 0xcb70, 0xc7e1,
        0xc434, 0xc068, 0xbc7f, 0xb87b, 0xb45d, 0xb026, 0xabd8, 0xa775, 0xa300, 0x9e78, 0x99e2,
        0x953f, 0x9092, 0x8bdc, 0x8720, 0x8260, 0x7d9f, 0x78e0, 0x7424, 0x6f6e, 0x6ac0, 0x661d,
        0x6187, 0x5d00, 0x588a, 0x5427, 0x4fda, 0x4ba3, 0x4784, 0x4380, 0x3f98, 0x3bcc, 0x381f,
        0x3490, 0x3122, 0x2dd4, 0x2aa8, 0x279e, 0x24b5, 0x21ef, 0x1f4c, 0x1ccb, 0x1a6b, 0x182e,
        0x1611, 0x1416, 0x123a, 0x107e, 0x0edf, 0x0d5f, 0x0bfa, 0x0ab1, 0x0982, 0x086d, 0x076f,
        0x0688, 0x05b6, 0x04f8, 0x044e, 0x03b5, 0x032d, 0x02b3, 0x0248, 0x01ea, 0x0198, 0x0151,
        0x0114, 0x00df, 0x00b2, 0x008d, 0x006d, 0x0053, 0x003e, 0x002d, 0x0020, 0x0016, 0x000e,
        0x0008, 0x0004, 0x0001};

    static constexpr size_t GRANULE_WINDOW_LENGTH =
        sizeof(GRANULE_WINDOW) / sizeof(GRANULE_WINDOW[0]);

    /**
     * import numpy as np
     * import scipy.signal as signal
     * window = np.cumsum(signal.windows.kaiser(40, beta=24))
     * window = np.clip(1.0 - window / window.max(), -1, 1)
     * window = np.floor(window * 2 ** 16)
     * elements = ", ".join([f"0x{int(x):04x}" for x in window])
     * print(f'static constexpr int FADE_WINDOW[{len(window)}] = {{ {elements} }};')
     */
    static constexpr int FADE_WINDOW[40] = {
        0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xfffc, 0xfff2, 0xffd6, 0xff8d, 0xfee8,
        0xfd96, 0xfb1e, 0xf6e1, 0xf025, 0xe634, 0xd885, 0xc6e7, 0xb1a8, 0x9998, 0x7fff,
        0x6667, 0x4e57, 0x3918, 0x277a, 0x19cb, 0x0fda, 0x091e, 0x04e1, 0x0269, 0x0117,
        0x0072, 0x0029, 0x000d, 0x0003, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

    static constexpr size_t FADE_WINDOW_LENGTH = sizeof(FADE_WINDOW) / sizeof(FADE_WINDOW[0]);

    struct StereoPair final
    {
      short l = 0;
      short r = 0;

      StereoPair() = default;
      StereoPair(short left, short right) : l(left), r(right) {}

      void mul(int fixed_16_left, int fixed_16_right);
      void mul(int fixed_16);

      StereoPair operator+(const StereoPair& other) const;

      static StereoPair interp(const StereoPair& s0, const StereoPair& s1, const StereoPair& s2,
                               const StereoPair& s3, const StereoPair& s4, const StereoPair& s5,
                               int t);
    };

    using GranuleBuffer = std::array<StereoPair, GRANULE_WINDOW_LENGTH>;
    static constexpr int GRANULE_BUFFER_BITS = 32 - std::countr_zero(GRANULE_WINDOW_LENGTH);

    class Granule final
    {
    private:
      GranuleBuffer m_buffer{};

    public:
      Granule() = default;
      Granule(const Granule&) = default;
      Granule& operator=(const Granule&) = default;
      Granule(Granule&&) = default;

      Granule(const GranuleBuffer& input, bool split);

      StereoPair operator[](uint32_t frac) const;

      void mul(int fixed_16_left, int fixed_16_right);
      void mul(int fixed_16);
    };

  public:
    MixerFifo(Mixer* mixer, unsigned sample_rate_divisor, bool little_endian)
        : m_mixer(mixer), m_input_sample_rate_divisor(sample_rate_divisor),
          m_little_endian(little_endian)
    {
    }
    void DoState(PointerWrap& p);
    void PushSamples(const short* samples, unsigned int num_samples);
    void Mix(short* samples, unsigned int num_samples);
    void SetInputSampleRateDivisor(unsigned int rate_divisor);
    unsigned int GetInputSampleRateDivisor() const;
    void SetVolume(unsigned int lvolume, unsigned int rvolume);
    std::pair<s32, s32> GetVolume() const;

  private:
    Mixer* m_mixer;
    unsigned m_input_sample_rate_divisor;
    bool m_little_endian;

    size_t m_buffer_index = 0;
    GranuleBuffer m_buffer{};

    uint32_t m_current_index;
    Granule m_front, m_back;

    std::array<Granule, GRANULE_QUEUE_SIZE> m_queue;
    std::atomic<size_t> m_queue_head{0};
    std::atomic<size_t> m_queue_tail{0};
    std::atomic<bool> m_queue_looping{false};
    size_t m_queue_fade_index = 0;

    void enqueue(const Granule& granule);
    void dequeue(Granule& granule);

    // Volume ranges from 0-256
    std::atomic<s32> m_LVolume{256};
    std::atomic<s32> m_RVolume{256};
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

  AudioCommon::SurroundDecoder m_surround_decoder;

  WaveFileWriter m_wave_writer_dtk;
  WaveFileWriter m_wave_writer_dsp;

  bool m_log_dtk_audio = false;
  bool m_log_dsp_audio = false;

  float m_config_emulation_speed;
  int m_config_timing_variance;

  Config::ConfigChangedCallbackID m_config_changed_callback_id;
};
