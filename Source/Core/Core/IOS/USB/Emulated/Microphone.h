// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <type_traits>

#include "Common/CommonTypes.h"

#ifdef HAVE_CUBEB
#include "AudioCommon/CubebUtils.h"

struct cubeb;
struct cubeb_stream;
#endif

namespace IOS::HLE::USB
{
class MicrophoneState
{
public:
  virtual ~MicrophoneState() = default;

  virtual bool IsSampleOn() const = 0;
  virtual bool IsMuted() const = 0;
  virtual u32 GetDefaultSamplingRate() const = 0;
};

class Microphone
{
public:
  using FloatType = float;
  using SampleType = s16;
  using UnsignedSampleType = std::make_unsigned_t<SampleType>;

  Microphone(const MicrophoneState& sampler, const std::string& worker_name);
  virtual ~Microphone();

  void Initialize();
  bool HasData(u32 sample_count) const;
  u16 ReadIntoBuffer(u8* ptr, u32 size);
  u16 GetLoudnessLevel() const;
  FloatType ComputeGain(FloatType relative_db) const;
  void SetSamplingRate(u32 sampling_rate);

protected:
  static constexpr u32 BUFF_SIZE_SAMPLES = 32;

private:
#ifdef HAVE_CUBEB
  static long CubebDataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                                void* output_buffer, long nframes);
  virtual std::string GetInputDeviceId() const = 0;
  virtual std::string GetCubebStreamName() const = 0;
  virtual s16 GetVolumeModifier() const = 0;
  virtual bool AreSamplesByteSwapped() const = 0;
#endif

  long DataCallback(const SampleType* input_buffer, long nframes);
  virtual bool IsMicrophoneMuted() const = 0;
  void UpdateLoudness(SampleType sample);

  void StreamInit();
  void StreamTerminate();
  void StreamStart(u32 sampling_rate);
  void StreamStop();

  virtual u32 GetStreamSize() const = 0;
  std::vector<SampleType> m_stream_buffer{};
  u32 m_stream_wpos = 0;
  u32 m_stream_rpos = 0;
  u32 m_samples_avail = 0;

  // TODO: Find how this level is calculated on real hardware
  std::atomic<u16> m_loudness_level = 0;
  struct Loudness
  {
    void Update(SampleType sample);

    SampleType GetPeak() const;
    static FloatType GetDecibel(FloatType value);
    FloatType GetAmplitude() const;
    FloatType GetAmplitudeDb() const;
    FloatType GetAbsoluteMean() const;
    FloatType GetAbsoluteMeanDb() const;
    FloatType GetRootMeanSquare() const;
    FloatType GetRootMeanSquareDb() const;
    FloatType GetCrestFactor() const;
    FloatType GetCrestFactorDb() const;
    static FloatType ComputeGain(FloatType db);

    void Reset();
    void LogStats();

    // Samples used to compute the loudness level (arbitrarily chosen)
    static constexpr u16 SAMPLES_NEEDED = 128;
    static_assert((SAMPLES_NEEDED % BUFF_SIZE_SAMPLES) == 0);

    static constexpr FloatType MAX_AMPLITUDE =
        UnsignedSampleType{std::numeric_limits<UnsignedSampleType>::max() / 2};
    static const FloatType DB_MIN;
    static const FloatType DB_MAX;

    u16 samples_count = 0;
    u32 absolute_sum = 0;
    FloatType square_sum = FloatType(0);
    SampleType peak_min = 0;
    SampleType peak_max = 0;
  };
  Loudness m_loudness;

  mutable std::mutex m_ring_lock;

  const MicrophoneState& m_sampler;

#ifdef HAVE_CUBEB
  std::shared_ptr<cubeb> m_cubeb_ctx = nullptr;
  cubeb_stream* m_cubeb_stream = nullptr;
  CubebUtils::CoInitSyncWorker m_worker;
#endif
};
}  // namespace IOS::HLE::USB
