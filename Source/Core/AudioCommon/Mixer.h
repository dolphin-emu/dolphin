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
     * window = np.cumsum(signal.windows.kaiser(128, beta=8.6))
     * window = np.concatenate([window, window[::-1]])
     * window = np.clip(window / window.max(), -1, 1)
     * window = np.round(window * 2 ** 16)
     * elements = ", ".join([f"0x{int(x):04x}" for x in window])
     * print(f'static constexpr int GRANULE_WINDOW[{len(window)}] = {{ {elements} }};')
     */
    static constexpr int GRANULE_WINDOW[256] = {
        0x0002, 0x0004, 0x0008, 0x000e, 0x0016, 0x0021, 0x002f,  0x0040,  0x0055, 0x0070, 0x0090,
        0x00b7, 0x00e4, 0x011a, 0x0159, 0x01a2, 0x01f6, 0x0257,  0x02c5,  0x0341, 0x03cd, 0x046a,
        0x0519, 0x05dc, 0x06b3, 0x07a0, 0x08a5, 0x09c2, 0x0af9,  0x0c4b,  0x0db9, 0x0f44, 0x10ed,
        0x12b5, 0x149d, 0x16a6, 0x18d0, 0x1b1b, 0x1d8a, 0x201b,  0x22ce,  0x25a5, 0x289f, 0x2bbb,
        0x2ef9, 0x3259, 0x35da, 0x397b, 0x3d3c, 0x411b, 0x4516,  0x492d,  0x4d5e, 0x51a7, 0x5607,
        0x5a7b, 0x5f02, 0x6399, 0x683f, 0x6cf1, 0x71ac, 0x766f,  0x7b36,  0x8000, 0x84ca, 0x8991,
        0x8e54, 0x930f, 0x97c1, 0x9c67, 0xa0fe, 0xa585, 0xa9f9,  0xae59,  0xb2a2, 0xb6d3, 0xbaea,
        0xbee5, 0xc2c4, 0xc685, 0xca26, 0xcda7, 0xd107, 0xd445,  0xd761,  0xda5b, 0xdd32, 0xdfe5,
        0xe276, 0xe4e5, 0xe730, 0xe95a, 0xeb63, 0xed4b, 0xef13,  0xf0bc,  0xf247, 0xf3b5, 0xf507,
        0xf63e, 0xf75b, 0xf860, 0xf94d, 0xfa24, 0xfae7, 0xfb96,  0xfc33,  0xfcbf, 0xfd3b, 0xfda9,
        0xfe0a, 0xfe5e, 0xfea7, 0xfee6, 0xff1c, 0xff49, 0xff70,  0xff90,  0xffab, 0xffc0, 0xffd1,
        0xffdf, 0xffea, 0xfff2, 0xfff8, 0xfffc, 0xfffe, 0x10000, 0x10000, 0xfffe, 0xfffc, 0xfff8,
        0xfff2, 0xffea, 0xffdf, 0xffd1, 0xffc0, 0xffab, 0xff90,  0xff70,  0xff49, 0xff1c, 0xfee6,
        0xfea7, 0xfe5e, 0xfe0a, 0xfda9, 0xfd3b, 0xfcbf, 0xfc33,  0xfb96,  0xfae7, 0xfa24, 0xf94d,
        0xf860, 0xf75b, 0xf63e, 0xf507, 0xf3b5, 0xf247, 0xf0bc,  0xef13,  0xed4b, 0xeb63, 0xe95a,
        0xe730, 0xe4e5, 0xe276, 0xdfe5, 0xdd32, 0xda5b, 0xd761,  0xd445,  0xd107, 0xcda7, 0xca26,
        0xc685, 0xc2c4, 0xbee5, 0xbaea, 0xb6d3, 0xb2a2, 0xae59,  0xa9f9,  0xa585, 0xa0fe, 0x9c67,
        0x97c1, 0x930f, 0x8e54, 0x8991, 0x84ca, 0x8000, 0x7b36,  0x766f,  0x71ac, 0x6cf1, 0x683f,
        0x6399, 0x5f02, 0x5a7b, 0x5607, 0x51a7, 0x4d5e, 0x492d,  0x4516,  0x411b, 0x3d3c, 0x397b,
        0x35da, 0x3259, 0x2ef9, 0x2bbb, 0x289f, 0x25a5, 0x22ce,  0x201b,  0x1d8a, 0x1b1b, 0x18d0,
        0x16a6, 0x149d, 0x12b5, 0x10ed, 0x0f44, 0x0db9, 0x0c4b,  0x0af9,  0x09c2, 0x08a5, 0x07a0,
        0x06b3, 0x05dc, 0x0519, 0x046a, 0x03cd, 0x0341, 0x02c5,  0x0257,  0x01f6, 0x01a2, 0x0159,
        0x011a, 0x00e4, 0x00b7, 0x0090, 0x0070, 0x0055, 0x0040,  0x002f,  0x0021, 0x0016, 0x000e,
        0x0008, 0x0004, 0x0002};

    static constexpr size_t GRANULE_WINDOW_LENGTH =
        sizeof(GRANULE_WINDOW) / sizeof(GRANULE_WINDOW[0]);

    /**
     * import numpy as np
     * import scipy.signal as signal
     * window = np.cumsum(signal.windows.kaiser(40, beta=24))
     * window = np.clip(1.0 - window / window.max(), -1, 1)
     * window = np.round(window * 2 ** 16)
     * elements = ", ".join([f"0x{int(x):04x}" for x in window])
     * print(f'static constexpr int FADE_WINDOW[{len(window)}] = {{ {elements} }};')
     */
    static constexpr int FADE_WINDOW[40] = {
        0x10000, 0x10000, 0x10000, 0x10000, 0xffff, 0xfffc, 0xfff3, 0xffd6, 0xff8d, 0xfee8,
        0xfd96,  0xfb1f,  0xf6e1,  0xf025,  0xe634, 0xd885, 0xc6e8, 0xb1a8, 0x9999, 0x8000,
        0x6667,  0x4e58,  0x3918,  0x277b,  0x19cc, 0x0fdb, 0x091f, 0x04e1, 0x026a, 0x0118,
        0x0073,  0x002a,  0x000d,  0x0004,  0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

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
