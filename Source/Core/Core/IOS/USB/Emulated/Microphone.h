// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <ranges>
#include <type_traits>

#include "AudioCommon/CubebUtils.h"
#include "Common/CommonTypes.h"

struct cubeb;
struct cubeb_stream;

namespace IOS::HLE::USB
{
struct WiiSpeakState;

class Microphone final
{
public:
  using FloatType = float;

  Microphone(const WiiSpeakState& sampler);
  ~Microphone();

  bool HasData(u32 sample_count) const;
  u16 ReadIntoBuffer(u8* ptr, u32 size);
  u16 GetLoudnessLevel() const;
  void UpdateLoudness(std::ranges::input_range auto&& samples);
  const WiiSpeakState& GetSampler() const;
  FloatType ComputeGain(FloatType relative_db) const;

private:
  static long DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                           void* output_buffer, long nframes);

  void StreamInit();
  void StreamTerminate();
  void StreamStart();
  void StopStream();

  static constexpr u32 SAMPLING_RATE = 8000;
  using SampleType = s16;
  static constexpr u32 BUFF_SIZE_SAMPLES = 16;
  static constexpr u32 STREAM_SIZE = BUFF_SIZE_SAMPLES * 500;

  std::array<SampleType, STREAM_SIZE> m_stream_buffer{};
  u32 m_stream_wpos = 0;
  u32 m_stream_rpos = 0;
  u32 m_samples_avail = 0;

  // TODO: Find how this level is calculated on real hardware
  u16 m_loudness_level = 0;
  struct Loudness
  {
    using SampleType = s16;
    using UnsignedSampleType = std::make_unsigned_t<SampleType>;

    void Update(const auto& samples)
    {
      samples_count += static_cast<u16>(samples.size());

      const auto [min_element, max_element] = std::ranges::minmax_element(samples);
      peak_min = std::min<SampleType>(*min_element, peak_min);
      peak_max = std::max<SampleType>(*max_element, peak_max);

      const auto begin = samples.begin();
      const auto end = samples.end();
      absolute_sum = std::reduce(begin, end, absolute_sum,
                                 [](u32 a, SampleType b) { return a + std::abs(b); });
      square_sum = std::reduce(begin, end, square_sum, [](FloatType a, s16 b) {
        return a + std::pow(FloatType(b), FloatType(2));
      });
    }

    SampleType GetPeak() const;
    FloatType GetDecibel(FloatType value) const;
    FloatType GetAmplitude() const;
    FloatType GetAmplitudeDb() const;
    FloatType GetAbsoluteMean() const;
    FloatType GetAbsoluteMeanDb() const;
    FloatType GetRootMeanSquare() const;
    FloatType GetRootMeanSquareDb() const;
    FloatType GetCrestFactor() const;
    FloatType GetCrestFactorDb() const;
    FloatType ComputeGain(FloatType db) const;

    void Reset();
    void LogStats();

    // Samples used to compute the loudness level
    static constexpr u16 SAMPLES_NEEDED = SAMPLING_RATE / 125;
    static_assert((SAMPLES_NEEDED % BUFF_SIZE_SAMPLES) == 0);

    static constexpr FloatType MAX_AMPLTIUDE = std::numeric_limits<UnsignedSampleType>::max() / 2;
    static const FloatType DB_MIN;
    static const FloatType DB_MAX;

    u16 samples_count = 0;
    u32 absolute_sum = 0;
    FloatType square_sum = FloatType(0);
    SampleType peak_min = 0;
    SampleType peak_max = 0;
  };
  Loudness m_loudness;

  std::mutex m_ring_lock;
  std::shared_ptr<cubeb> m_cubeb_ctx = nullptr;
  cubeb_stream* m_cubeb_stream = nullptr;

  const WiiSpeakState& m_sampler;

  CubebUtils::CoInitSyncWorker m_worker{"Wii Speak Worker"};
};
}  // namespace IOS::HLE::USB
