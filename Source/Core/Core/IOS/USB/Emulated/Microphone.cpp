// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Microphone.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <span>

#ifdef HAVE_CUBEB
#include <cubeb/cubeb.h>

#include "AudioCommon/CubebUtils.h"
#endif

#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"
#include "Core/Core.h"
#include "Core/System.h"

#ifdef _WIN32
#include <Objbase.h>
#endif

#ifdef ANDROID
#include "jni/AndroidCommon/IDCache.h"
#endif

namespace IOS::HLE::USB
{
#ifdef HAVE_CUBEB
Microphone::Microphone(const MicrophoneState& sampler, const std::string& worker_name)
    : m_sampler(sampler), m_worker(worker_name)
{
}
#else
Microphone::Microphone(const MicrophoneState& sampler, const std::string& worker_name)
    : m_sampler(sampler)
{
}
#endif

Microphone::~Microphone()
{
  StreamTerminate();
}

void Microphone::Initialize()
{
  StreamInit();
}

#ifndef HAVE_CUBEB
void Microphone::StreamInit()
{
}

void Microphone::StreamStart([[maybe_unused]] u32 sampling_rate)
{
}

void Microphone::StreamStop()
{
}

void Microphone::StreamTerminate()
{
}
#else
void Microphone::StreamInit()
{
  if (!m_worker.Execute([this] { m_cubeb_ctx = CubebUtils::GetContext(); }))
  {
    ERROR_LOG_FMT(IOS_USB, "Failed to init microphone stream");
    return;
  }

  // TODO: Not here but rather inside the WiiSpeak device if possible?
  StreamStart(m_sampler.GetDefaultSamplingRate());
}

void Microphone::StreamTerminate()
{
  StreamStop();

  if (m_cubeb_ctx)
    m_worker.Execute([this] { m_cubeb_ctx.reset(); });
}

static void StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state)
{
}

void Microphone::StreamStart(u32 sampling_rate)
{
  if (!m_cubeb_ctx)
    return;

  m_worker.Execute([this, sampling_rate] {
#ifdef ANDROID
    JNIEnv* env = IDCache::GetEnvForThread();
    if (jboolean result = env->CallStaticBooleanMethod(
            IDCache::GetPermissionHandlerClass(),
            IDCache::GetPermissionHandlerHasRecordAudioPermission(), nullptr);
        result == JNI_FALSE)
    {
      env->CallStaticVoidMethod(IDCache::GetPermissionHandlerClass(),
                                IDCache::GetPermissionHandlerRequestRecordAudioPermission(),
                                nullptr);
    }
#endif

    cubeb_stream_params params{};
    params.format = CUBEB_SAMPLE_S16LE;
    params.rate = sampling_rate;
    params.channels = 1;
    params.layout = CUBEB_LAYOUT_MONO;

    std::lock_guard lock(m_ring_lock);
    u32 minimum_latency;
    if (cubeb_get_min_latency(m_cubeb_ctx.get(), &params, &minimum_latency) != CUBEB_OK)
    {
      WARN_LOG_FMT(IOS_USB, "Error getting minimum latency");
      minimum_latency = 16;
    }

    cubeb_devid input_device = CubebUtils::GetInputDeviceById(GetInputDeviceId());
    if (cubeb_stream_init(m_cubeb_ctx.get(), &m_cubeb_stream, GetCubebStreamName().c_str(),
                          input_device, &params, nullptr, nullptr,
                          std::max<u32>(16, minimum_latency), CubebDataCallback, StateCallback,
                          this) != CUBEB_OK)
    {
      ERROR_LOG_FMT(IOS_USB, "Error initializing cubeb stream");
      return;
    }

    if (cubeb_stream_start(m_cubeb_stream) != CUBEB_OK)
    {
      ERROR_LOG_FMT(IOS_USB, "Error starting cubeb stream");
      return;
    }

    m_stream_buffer.resize(GetStreamSize());
    m_stream_wpos = 0;
    INFO_LOG_FMT(IOS_USB, "started cubeb stream");
  });
}

void Microphone::StreamStop()
{
  if (!m_cubeb_stream)
    return;

  m_worker.Execute([this] {
    if (cubeb_stream_stop(m_cubeb_stream) != CUBEB_OK)
      ERROR_LOG_FMT(IOS_USB, "Error stopping cubeb stream");
    cubeb_stream_destroy(m_cubeb_stream);
    m_cubeb_stream = nullptr;
  });
}

long Microphone::CubebDataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                                   void* /*output_buffer*/, long nframes)
{
  // Skip data when core isn't running
  if (Core::GetState(Core::System::GetInstance()) != Core::State::Running)
    return nframes;

  auto* mic = static_cast<Microphone*>(user_data);

  // Skip data when HLE Wii Speak is muted
  // TODO: Update cubeb and use cubeb_stream_set_input_mute
  if (mic->IsMicrophoneMuted())
    return nframes;

  return mic->DataCallback(static_cast<const SampleType*>(input_buffer), nframes);
}

long Microphone::DataCallback(const SampleType* input_buffer, long nframes)
{
  std::lock_guard lock(m_ring_lock);

  // Skip data if sampling is off or mute is on
  if (!m_sampler.IsSampleOn() || m_sampler.IsMuted())
    return nframes;

  std::span<const SampleType> buffer(input_buffer, nframes);
  const auto gain = ComputeGain(GetVolumeModifier());
  const auto apply_gain = [gain](SampleType sample) {
    return MathUtil::SaturatingCast<SampleType>(sample * gain);
  };

  const u32 stream_size = GetStreamSize();
  const bool are_samples_byte_swapped = AreSamplesByteSwapped();
  for (const SampleType le_sample : std::ranges::transform_view(buffer, apply_gain))
  {
    UpdateLoudness(le_sample);
    m_stream_buffer[m_stream_wpos] =
        are_samples_byte_swapped ? Common::swap16(le_sample) : le_sample;
    m_stream_wpos = (m_stream_wpos + 1) % stream_size;
  }

  m_samples_avail += nframes;
  if (m_samples_avail > stream_size)
  {
    m_samples_avail = stream_size;
  }

  return nframes;
}
#endif

u16 Microphone::ReadIntoBuffer(u8* ptr, u32 size)
{
  static constexpr u32 SINGLE_READ_SIZE = BUFF_SIZE_SAMPLES * sizeof(SampleType);

  std::lock_guard lock(m_ring_lock);

  const u32 stream_size = GetStreamSize();
  u8* begin = ptr;
  for (u8* end = begin + size; ptr < end; ptr += SINGLE_READ_SIZE, size -= SINGLE_READ_SIZE)
  {
    if (size < SINGLE_READ_SIZE || m_samples_avail < BUFF_SIZE_SAMPLES)
      break;

    SampleType* last_buffer = &m_stream_buffer[m_stream_rpos];
    std::memcpy(ptr, last_buffer, SINGLE_READ_SIZE);

    m_samples_avail -= BUFF_SIZE_SAMPLES;
    m_stream_rpos += BUFF_SIZE_SAMPLES;
    m_stream_rpos %= stream_size;
  }
  return static_cast<u16>(ptr - begin);
}

u16 Microphone::GetLoudnessLevel() const
{
  if (m_sampler.IsMuted() || IsMicrophoneMuted())
    return 0;
  return m_loudness_level;
}

// Based on graphical cues on Monster Hunter 3, the level seems properly displayed with values
// between 0 and 0x3a00.
//
// TODO: Proper hardware testing, documentation, formulas...
void Microphone::UpdateLoudness(const SampleType sample)
{
  // Based on MH3 graphical cues, let's use a 0x4000 window
  static const u32 WINDOW = 0x4000;
  static const FloatType UNIT = (m_loudness.DB_MAX - m_loudness.DB_MIN) / WINDOW;

  m_loudness.Update(sample);

  if (m_loudness.samples_count >= m_loudness.SAMPLES_NEEDED)
  {
    const FloatType amp_db = m_loudness.GetAmplitudeDb();
    m_loudness_level = static_cast<u16>((amp_db - m_loudness.DB_MIN) / UNIT);

#ifdef WII_SPEAK_LOG_STATS
    m_loudness.LogStats();
#endif

    m_loudness.Reset();
  }
}

bool Microphone::HasData(u32 sample_count = BUFF_SIZE_SAMPLES) const
{
  std::lock_guard lock(m_ring_lock);
  return m_samples_avail >= sample_count;
}

Microphone::FloatType Microphone::ComputeGain(FloatType relative_db) const
{
  return m_loudness.ComputeGain(relative_db);
}

void Microphone::SetSamplingRate(u32 sampling_rate)
{
  StreamStop();
  StreamStart(sampling_rate);
}

const Microphone::FloatType Microphone::Loudness::DB_MIN =
    20 * std::log10(FloatType(1) / MAX_AMPLITUDE);
const Microphone::FloatType Microphone::Loudness::DB_MAX = 20 * std::log10(FloatType(1));

void Microphone::Loudness::Update(const SampleType sample)
{
  ++samples_count;

  peak_min = std::min(sample, peak_min);
  peak_max = std::max(sample, peak_max);
  absolute_sum += std::abs(sample);
  square_sum += std::pow(FloatType(sample), FloatType(2));
}

Microphone::SampleType Microphone::Loudness::GetPeak() const
{
  return std::max(std::abs(peak_min), std::abs(peak_max));
}

Microphone::FloatType Microphone::Loudness::GetDecibel(FloatType value)
{
  return 20 * std::log10(value);
}

Microphone::FloatType Microphone::Loudness::GetAmplitude() const
{
  return GetPeak() / MAX_AMPLITUDE;
}

Microphone::FloatType Microphone::Loudness::GetAmplitudeDb() const
{
  return GetDecibel(GetAmplitude());
}

Microphone::FloatType Microphone::Loudness::GetAbsoluteMean() const
{
  return FloatType(absolute_sum) / samples_count;
}

Microphone::FloatType Microphone::Loudness::GetAbsoluteMeanDb() const
{
  return GetDecibel(GetAbsoluteMean());
}

Microphone::FloatType Microphone::Loudness::GetRootMeanSquare() const
{
  return std::sqrt(square_sum / samples_count);
}

Microphone::FloatType Microphone::Loudness::GetRootMeanSquareDb() const
{
  return GetDecibel(GetRootMeanSquare());
}

Microphone::FloatType Microphone::Loudness::GetCrestFactor() const
{
  const auto rms = GetRootMeanSquare();
  if (rms == 0)
    return FloatType(0);
  return GetPeak() / rms;
}

Microphone::FloatType Microphone::Loudness::GetCrestFactorDb() const
{
  return GetDecibel(GetCrestFactor());
}

Microphone::FloatType Microphone::Loudness::ComputeGain(FloatType db)
{
  return std::pow(FloatType(10), db / 20);
}

void Microphone::Loudness::Reset()
{
  samples_count = 0;
  absolute_sum = 0;
  square_sum = FloatType(0);
  peak_min = 0;
  peak_max = 0;
}

void Microphone::Loudness::LogStats()
{
  const auto amplitude = GetAmplitude();
  const auto amplitude_db = GetDecibel(amplitude);
  const auto rms = GetRootMeanSquare();
  const auto rms_db = GetDecibel(rms);
  const auto abs_mean = GetAbsoluteMean();
  const auto abs_mean_db = GetDecibel(abs_mean);
  const auto crest_factor = GetCrestFactor();
  const auto crest_factor_db = GetDecibel(crest_factor);

  INFO_LOG_FMT(IOS_USB,
               "Microphone loudness stats (sample count: {}/{}):\n"
               " - min={} max={} amplitude={} ({} dB)\n"
               " - rms={} ({} dB) \n"
               " - abs_mean={} ({} dB)\n"
               " - crest_factor={} ({} dB)",
               samples_count, SAMPLES_NEEDED, peak_min, peak_max, amplitude, amplitude_db, rms,
               rms_db, abs_mean, abs_mean_db, crest_factor, crest_factor_db);
}
}  // namespace IOS::HLE::USB
