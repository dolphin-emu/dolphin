// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "AudioCommon/AudioStretcher.h"
#include "AudioCommon/AudioSpeedCounter.h"
#include "AudioCommon/SurroundDecoder.h"
#include "AudioCommon/WaveFile.h"
#include "Common/CommonTypes.h"

class PointerWrap;

class Mixer final
{
public:
  explicit Mixer(u32 sample_rate);
  ~Mixer();

  void DoState(PointerWrap& p);

  void SetPaused(bool paused);

  // Called from audio threads:
  u32 Mix(s16* samples, u32 num_samples);
  u32 MixSurround(float* samples, u32 num_samples);

  // Called from main thread:
  void PushDMASamples(const s16* samples, u32 num_samples);
  void PushStreamingSamples(const s16* samples, u32 num_samples);
  void PushWiimoteSpeakerSamples(u8 index, const s16* samples, u32 num_samples, u32 sample_rate);

  void SetDMAInputSampleRate(double rate);
  void SetStreamingInputSampleRate(double rate);

  void SetStreamingVolume(u32 lVolume, u32 rVolume);
  void SetWiimoteSpeakerVolume(u8 index, u32 lVolume, u32 rVolume);

  void StartLogDTKAudio(const std::string& filename);
  void StopLogDTKAudio();

  void StartLogDSPAudio(const std::string& filename);
  void StopLogDSPAudio();

  // Only call when the audio thread (Mix()) is not running
  void UpdateSettings(u32 sample_rate);
  // Useful to clean the surround buffer when we enable/disable it
  void SetSurroundChanged() { m_surround_changed = true; }

  u32 GetSampleRate() const { return m_sample_rate; }
  double GetCurrentSpeed() const { return m_target_speed; }
  double GetMixingSpeed() const { return m_mixing_speed; }
  double GetMinLatency() const { return m_min_latency; }
  double GetMaxLatency() const { return m_max_latency; }

  static void AdjustSpeedByLatency(double latency, double acceptable_latency, double min_latency,
                                   double max_latency, double catch_up_speed, double& target_speed,
                                   s8& latency_catching_up_direction);

  // 512ms at 32kHz and ~341ms at 48kHz (on Wii, GC has similar values).
  // Make sure this is a power of 2 for INDEX_MASK to work,
  // if not change all "& INDEX_MASK" to "% (MAX_SAMPLES * NC)".
  // It's important that this is high enough to allow for enough backwards
  // samples to be played in during a stutter. It doesn't make much sense that
  // this is independent from the sample rate, but it's fine.
  // If we ever wanted to allow extremely high emulation speeds, we'd need to increase this
  static constexpr u32 MAX_SAMPLES = 16384;

private:
  // Number of channels the mixer has. Shortened because it is constantly used.
  // The main reason we made this is to allow for faster conversion to another number
  // of channels, as most of the code is channel agnostic
  static constexpr u32 NC = 2u;
  static constexpr u32 SURROUND_CHANNELS = 6u;

  static constexpr u32 INDEX_MASK = MAX_SAMPLES * NC - 1;

  // Interpolation reserved/required samples
  static constexpr u32 INTERP_SAMPLES = 3u;

  static constexpr double NON_STRETCHING_CATCH_UP_SPEED = 1.0175;
  static constexpr double STRETCHING_CATCH_UP_SPEED = 1.25;

  // A single mixer in/out buffer. The original alignment they might have had on real HW isn't kept
  class MixerFifo final
  {
  public:
    MixerFifo(Mixer* mixer, unsigned sample_rate, bool big_endians, bool constantly_pushed = true)
        : m_mixer(mixer), m_input_sample_rate(sample_rate),
          m_big_endians(big_endians), m_constantly_pushed(constantly_pushed)
    {
    }
    void DoState(PointerWrap& p);
    void PushSamples(const s16* samples, u32 num_samples);
    // Returns the actual mixed samples num, pads the rest with the last sample.
    // Executed from sound stream thread
    u32 Mix(s16* samples, u32 num_samples, bool stretching = false);
    void SetInputSampleRate(double sample_rate);
    double GetInputSampleRate() const;
    u32 GetRoundedInputSampleRate() const;
    bool IsCurrentlyPushed() const { return m_currently_pushed; }
    // Expects values from 0 to 255
    void SetVolume(u32 lVolume, u32 rVolume);
    // Returns the max number of samples we will be able to mix (in output sample rate).
    // This is not precise due to the interpolation fract, but it's close enough
    u32 AvailableSamples() const;
    u32 NumSamples() const;
    u32 SamplesDifference(u32 indexW, u32 indexR) const;
    u32 SamplesDifference(u32 indexW, u32 indexR, double rate, double fract) const;
    u32 GetNextIndexR(u32 indexR, double rate, double fract) const;
    void UpdatePush(double time);
    
    // Returns the actual number of samples written. Outputs the last played sample for padding.
    // It's in int32 range because the interpolation can produce values over int16 and we don't want
    // to lose the extra
    u32 CubicInterpolation(s16* samples, u32 num_samples, double rate, u32& indexR, u32 indexW,
                           s32& l_s, s32& r_s, s32 lVolume, s32 rVolume, bool forwards = true);

  private:
    Mixer* m_mixer;
    std::atomic<double> m_input_sample_rate;
    // Ring input buffer directly from emulated HW. Might be big endians.
    // Even indexes are right channel, as opposed to the output
    std::array<s16, MAX_SAMPLES * NC> m_buffer{};

    // m_buffer indexes
    // Write: how many have been written, index of the next one to write.
    // Starts from INTERP_SAMPLES + 1 so that we gradually blend into the initial samples.
    // Read: how many have been read - 1, index of last one to have been read and that we are
    // currently interpolating.
    // If their difference is 0, we will have read all the samples (or there are none left anyway).
    // We could still re-read the current indexR to get the last sample value (0 if never written).
    // This is because we don't want to increase indexR over indexW to tell that we have finished
    // reading the pushed samples, it would be confusing. They loop over the max range fine.
    std::atomic<u32> m_indexW{(INTERP_SAMPLES + 1) * NC};
    std::atomic<u32> m_indexR{0};
    u32 m_backwards_indexR = 0;  // Keep track of backwards playing samples

    // See comment on CubicInterpolation()
    s32 m_last_output_samples[NC]{};
    // In and out rate might not be 1, this is the fract position we are reading between 2 samples.
    // We only write this from the audio thread.
    // It would be nice to reset once in a while, like on DoState(), or when the in/out sample rates
    // change, as it can fall out of alignment, but it would be wrong and the gains would be minimal
    std::atomic<double> m_fract{-1.0};
    double m_backwards_fract = -1.0;  // Doesn't need to be atomic
    s8 m_latency_catching_up_direction = 0;

    // Volume range is 0-256
    std::atomic<s32> m_lVolume{256};
    std::atomic<s32> m_rVolume{256};
    // Some mixers don't constantly push new samples and might push at arbitrary times,
    // in that case, we can't play samples backwards and we need some special code to handle
    // the max/min latency
    const bool m_constantly_pushed;
    std::atomic<bool> m_currently_pushed{false};
    double m_last_push_timer = -1.0;
    const bool m_big_endians;
  };

  MixerFifo m_dma_mixer{this, 32000, true};
  MixerFifo m_streaming_mixer{this, 48000, true};
  MixerFifo m_wiimote_speaker_mixer[4]{{this, 6000, false, false},
                                       {this, 6000, false, false},
                                       {this, 6000, false, false},
                                       {this, 6000, false, false}};

  // The size is always left at 0, they are never initialed, we use memset
  std::vector<s16> m_scratch_buffer;
  std::array<s16, MAX_SAMPLES * NC> m_interpolation_buffer;
  s16 m_conversion_buffer[MAX_SAMPLES * NC];

  // Target mixing (playback) speed, it's usually equal the target emulation speed
  std::atomic<double> m_target_speed{1.0};
  // Same as m_target_speed, but set to 1 when stretching as that takes care of speed
  std::atomic<double> m_mixing_speed{1.0};
  double m_time_behind_target_speed = 0.0;
  bool m_behind_target_speed = false;
  std::atomic<double> m_time_at_custom_speed{0.0};
  s8 m_stretching_latency_catching_up_direction = 0;
  std::atomic<double> m_backend_latency{0.0};
  std::atomic<double> m_last_mix_time{0.0};
  double m_min_latency;
  double m_max_latency;
  // Average of the last 0.425 seconds, a good balance between reactiveness and smoothness.
  // Backend latency should be <= this to work well.
  // Start at the most common sample rate and pushed samples num per batch
  AudioSpeedCounter m_dma_speed{0.425, 32000, 560};

  u32 m_sample_rate;  // Only changed by main or emulation thread when the backend is not running
  bool m_stretching = false;
  std::atomic<bool> m_surround_changed{false};
  bool m_disabling_surround = false;
  bool m_update_surround_latency = false;
  AudioCommon::AudioStretcher m_stretcher;
  AudioCommon::SurroundDecoder m_surround_decoder;

  int m_on_state_changed_handle = -1;

  WaveFileWriter m_wave_writer_dtk;
  WaveFileWriter m_wave_writer_dsp;

  bool m_log_dtk_audio = false;
  bool m_log_dsp_audio = false;
};
