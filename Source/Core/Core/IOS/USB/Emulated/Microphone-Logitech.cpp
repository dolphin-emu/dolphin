// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Microphone-Logitech.h"

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
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Emulated/LogitechMic.h"
#include "Core/System.h"

#ifdef _WIN32
#include <Objbase.h>
#endif

#ifdef ANDROID
#include "jni/AndroidCommon/IDCache.h"
#endif

namespace IOS::HLE::USB
{
MicrophoneLogitech::MicrophoneLogitech(const LogitechMicState& sampler) : m_sampler(sampler)
{
  StreamInit();
}

MicrophoneLogitech::~MicrophoneLogitech()
{
  StreamTerminate();
}

#ifndef HAVE_CUBEB
void MicrophoneLogitech::StreamInit()
{
}

void MicrophoneLogitech::StreamStart([[maybe_unused]] u32 sampling_rate)
{
}

void MicrophoneLogitech::StreamStop()
{
}

void MicrophoneLogitech::StreamTerminate()
{
}
#else
void MicrophoneLogitech::StreamInit()
{
  if (!m_worker.Execute([this] { m_cubeb_ctx = CubebUtils::GetContext(); }))
  {
    ERROR_LOG_FMT(IOS_USB, "Failed to init Logitech Mic stream");
    return;
  }

  // TODO: Not here but rather inside the WiiSpeak device if possible?
  StreamStart(m_sampler.DEFAULT_SAMPLING_RATE);
}

void MicrophoneLogitech::StreamTerminate()
{
  StreamStop();

  if (m_cubeb_ctx)
    m_worker.Execute([this] { m_cubeb_ctx.reset(); });
}

static void StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state)
{
}

void MicrophoneLogitech::StreamStart(u32 sampling_rate)
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

    u32 minimum_latency;
    if (cubeb_get_min_latency(m_cubeb_ctx.get(), &params, &minimum_latency) != CUBEB_OK)
    {
      WARN_LOG_FMT(IOS_USB, "Error getting minimum latency");
      minimum_latency = 16;
    }

    cubeb_devid input_device =
        CubebUtils::GetInputDeviceById(Config::Get(Config::MAIN_WII_SPEAK_MICROPHONE));
    if (cubeb_stream_init(m_cubeb_ctx.get(), &m_cubeb_stream, "Dolphin Emulated Logitech Mic",
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

    INFO_LOG_FMT(IOS_USB, "started cubeb stream");
  });
}

void MicrophoneLogitech::StreamStop()
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

long MicrophoneLogitech::CubebDataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                                   void* /*output_buffer*/, long nframes)
{
  // Skip data when core isn't running
  if (Core::GetState(Core::System::GetInstance()) != Core::State::Running)
    return nframes;

  // Skip data when HLE Wii Speak is muted
  // TODO: Update cubeb and use cubeb_stream_set_input_mute
  if (Config::Get(Config::MAIN_LOGITECH_MIC_MUTED))
    return nframes;

  auto* mic = static_cast<MicrophoneLogitech*>(user_data);
  return mic->DataCallback(static_cast<const SampleType*>(input_buffer), nframes);
}

long MicrophoneLogitech::DataCallback(const SampleType* input_buffer, long nframes)
{
  std::lock_guard lock(m_ring_lock);

  // Skip data if sampling is off or mute is on
  if (!m_sampler.sample_on || m_sampler.mute)
    return nframes;

  std::span<const SampleType> buffer(input_buffer, nframes);
  const auto gain = ComputeGain(Config::Get(Config::MAIN_LOGITECH_MIC_VOLUME_MODIFIER));
  const auto apply_gain = [gain](SampleType sample) {
    return MathUtil::SaturatingCast<SampleType>(sample * gain);
  };

  for (const SampleType le_sample : std::ranges::transform_view(buffer, apply_gain))
  {
    UpdateLoudness(le_sample);
    m_stream_buffer[m_stream_wpos] = Common::swap16(le_sample);
    m_stream_wpos = (m_stream_wpos + 1) % STREAM_SIZE;
  }

  m_samples_avail += nframes;
  if (m_samples_avail > STREAM_SIZE)
  {
    WARN_LOG_FMT(IOS_USB, "Logitech Mic ring buffer is full, data will be lost!");
    m_samples_avail = STREAM_SIZE;
  }

  return nframes;
}
#endif

u16 MicrophoneLogitech::ReadIntoBuffer(u8* ptr, u32 size)
{
  static constexpr u32 SINGLE_READ_SIZE = BUFF_SIZE_SAMPLES * sizeof(SampleType);

  // Avoid buffer overflow during memcpy
  static_assert((STREAM_SIZE % BUFF_SIZE_SAMPLES) == 0,
                "The STREAM_SIZE isn't a multiple of BUFF_SIZE_SAMPLES");

  std::lock_guard lock(m_ring_lock);

  u8* begin = ptr;
  for (u8* end = begin + size; ptr < end; ptr += SINGLE_READ_SIZE, size -= SINGLE_READ_SIZE)
  {
    if (size < SINGLE_READ_SIZE || m_samples_avail < BUFF_SIZE_SAMPLES)
      break;

    SampleType* last_buffer = &m_stream_buffer[m_stream_rpos];
    std::memcpy(ptr, last_buffer, SINGLE_READ_SIZE);

    m_samples_avail -= BUFF_SIZE_SAMPLES;
    m_stream_rpos += BUFF_SIZE_SAMPLES;
    m_stream_rpos %= STREAM_SIZE;
  }
  return static_cast<u16>(ptr - begin);
}

u16 MicrophoneLogitech::GetLoudnessLevel() const
{
  if (m_sampler.mute || Config::Get(Config::MAIN_LOGITECH_MIC_MUTED))
    return 0;
  return m_loudness_level;
}

// Based on graphical cues on Monster Hunter 3, the level seems properly displayed with values
// between 0 and 0x3a00.
//
// TODO: Proper hardware testing, documentation, formulas...
void MicrophoneLogitech::UpdateLoudness(const SampleType sample)
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

bool MicrophoneLogitech::HasData(u32 sample_count = BUFF_SIZE_SAMPLES) const
{
  std::lock_guard lock(m_ring_lock);
  return m_samples_avail >= sample_count;
}

MicrophoneLogitech::FloatType MicrophoneLogitech::ComputeGain(FloatType relative_db) const
{
  return m_loudness.ComputeGain(relative_db);
}

void MicrophoneLogitech::SetSamplingRate(u32 sampling_rate)
{
  StreamStop();
  StreamStart(sampling_rate);
}

const MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::DB_MIN =
    20 * std::log10(FloatType(1) / MAX_AMPLITUDE);
const MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::DB_MAX = 20 * std::log10(FloatType(1));

void MicrophoneLogitech::Loudness::Update(const SampleType sample)
{
  ++samples_count;

  peak_min = std::min(sample, peak_min);
  peak_max = std::max(sample, peak_max);
  absolute_sum += std::abs(sample);
  square_sum += std::pow(FloatType(sample), FloatType(2));
}

MicrophoneLogitech::SampleType MicrophoneLogitech::Loudness::GetPeak() const
{
  return std::max(std::abs(peak_min), std::abs(peak_max));
}

MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::GetDecibel(FloatType value)
{
  return 20 * std::log10(value);
}

MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::GetAmplitude() const
{
  return GetPeak() / MAX_AMPLITUDE;
}

MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::GetAmplitudeDb() const
{
  return GetDecibel(GetAmplitude());
}

MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::GetAbsoluteMean() const
{
  return FloatType(absolute_sum) / samples_count;
}

MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::GetAbsoluteMeanDb() const
{
  return GetDecibel(GetAbsoluteMean());
}

MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::GetRootMeanSquare() const
{
  return std::sqrt(square_sum / samples_count);
}

MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::GetRootMeanSquareDb() const
{
  return GetDecibel(GetRootMeanSquare());
}

MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::GetCrestFactor() const
{
  const auto rms = GetRootMeanSquare();
  if (rms == 0)
    return FloatType(0);
  return GetPeak() / rms;
}

MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::GetCrestFactorDb() const
{
  return GetDecibel(GetCrestFactor());
}

MicrophoneLogitech::FloatType MicrophoneLogitech::Loudness::ComputeGain(FloatType db)
{
  return std::pow(FloatType(10), db / 20);
}

void MicrophoneLogitech::Loudness::Reset()
{
  samples_count = 0;
  absolute_sum = 0;
  square_sum = FloatType(0);
  peak_min = 0;
  peak_max = 0;
}

void MicrophoneLogitech::Loudness::LogStats()
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
               "Logitech Mic loudness stats (sample count: {}/{}):\n"
               " - min={} max={} amplitude={} ({} dB)\n"
               " - rms={} ({} dB) \n"
               " - abs_mean={} ({} dB)\n"
               " - crest_factor={} ({} dB)",
               samples_count, SAMPLES_NEEDED, peak_min, peak_max, amplitude, amplitude_db, rms,
               rms_db, abs_mean, abs_mean_db, crest_factor, crest_factor_db);
}
}  // namespace IOS::HLE::USB
