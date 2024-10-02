// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Microphone.h"

#include <span>

#include <cubeb/cubeb.h>

#include "AudioCommon/CubebUtils.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Emulated/WiiSpeak.h"

#ifdef _WIN32
#include <Objbase.h>
#endif

#ifdef ANDROID
#include "jni/AndroidCommon/IDCache.h"
#endif

namespace IOS::HLE::USB
{
Microphone::Microphone(const WiiSpeakState& sampler) : m_sampler(sampler)
{
  StreamInit();
}

Microphone::~Microphone()
{
  StreamTerminate();
}

void Microphone::StreamInit()
{
  if (!m_worker.Execute([this] { m_cubeb_ctx = CubebUtils::GetContext(); }))
  {
    ERROR_LOG_FMT(IOS_USB, "Failed to init Wii Speak stream");
    return;
  }

  // TODO: Not here but rather inside the WiiSpeak device if possible?
  StreamStart();
}

void Microphone::StreamTerminate()
{
  StopStream();

  if (m_cubeb_ctx)
    m_worker.Execute([this] { m_cubeb_ctx.reset(); });
}

static void state_callback(cubeb_stream* stream, void* user_data, cubeb_state state)
{
}

void Microphone::StreamStart()
{
  if (!m_cubeb_ctx)
    return;

  m_worker.Execute([this] {
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
    params.rate = SAMPLING_RATE;
    params.channels = 1;
    params.layout = CUBEB_LAYOUT_MONO;

    u32 minimum_latency;
    if (cubeb_get_min_latency(m_cubeb_ctx.get(), &params, &minimum_latency) != CUBEB_OK)
    {
      WARN_LOG_FMT(IOS_USB, "Error getting minimum latency");
    }

    cubeb_devid input_device =
        CubebUtils::GetInputDeviceById(Config::Get(Config::MAIN_WII_SPEAK_MICROPHONE));
    if (cubeb_stream_init(m_cubeb_ctx.get(), &m_cubeb_stream, "Dolphin Emulated Wii Speak",
                          input_device, &params, nullptr, nullptr,
                          std::max<u32>(16, minimum_latency), DataCallback, state_callback,
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

void Microphone::StopStream()
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

long Microphone::DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                              void* /*output_buffer*/, long nframes)
{
  // Skip data when core isn't running
  if (Core::GetState(Core::System::GetInstance()) != Core::State::Running)
    return nframes;

  // Skip data when HLE Wii Speak is muted
  // TODO: Update cubeb and use cubeb_stream_set_input_mute
  if (Config::Get(Config::MAIN_WII_SPEAK_MUTED))
    return nframes;

  auto* mic = static_cast<Microphone*>(user_data);
  const auto& sampler = mic->GetSampler();

  // Skip data if sampling is off or mute is on
  if (!sampler.sample_on || sampler.mute)
    return nframes;

  std::lock_guard lk(mic->m_ring_lock);
  std::span<const s16> buff_in(static_cast<const s16*>(input_buffer), nframes);
  const auto gain = mic->ComputeGain(Config::Get(Config::MAIN_WII_SPEAK_VOLUME_MODIFIER));
  auto processed_buff_in = buff_in | std::views::transform([gain](s16 sample) {
                             return MathUtil::SaturatingCast<s16>(sample * gain);
                           });

  mic->UpdateLoudness(processed_buff_in);

  for (s16 le_sample : processed_buff_in)
  {
    mic->m_stream_buffer[mic->m_stream_wpos] = Common::swap16(le_sample);
    mic->m_stream_wpos = (mic->m_stream_wpos + 1) % mic->STREAM_SIZE;
  }

  mic->m_samples_avail += nframes;
  if (mic->m_samples_avail > mic->STREAM_SIZE)
  {
    WARN_LOG_FMT(IOS_USB, "Wii Speak ring buffer is full, data will be lost!");
    mic->m_samples_avail = 0;
  }

  return nframes;
}

u16 Microphone::ReadIntoBuffer(u8* ptr, u32 size)
{
  static constexpr u32 SINGLE_READ_SIZE = BUFF_SIZE_SAMPLES * sizeof(SampleType);

  // Avoid buffer overflow during memcpy
  static_assert((STREAM_SIZE % BUFF_SIZE_SAMPLES) == 0,
                "The STREAM_SIZE isn't a multiple of BUFF_SIZE_SAMPLES");

  std::lock_guard lk(m_ring_lock);

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

u16 Microphone::GetLoudnessLevel() const
{
  if (m_sampler.mute || Config::Get(Config::MAIN_WII_SPEAK_MUTED))
    return 0;
  return m_loudness_level;
}

// Based on graphical cues on Monster Hunter 3, the level seems properly displayed with values
// between 0 and 0x3a00.
//
// TODO: Proper hardware testing, documentation, formulas...
void Microphone::UpdateLoudness(std::ranges::input_range auto&& samples)
{
  // Based on MH3 graphical cues, let's use a 0x4000 window
  static const u32 WINDOW = 0x4000;
  static const float UNIT = (m_loudness.DB_MAX - m_loudness.DB_MIN) / WINDOW;

  m_loudness.Update(samples);

  if (m_loudness.samples_count >= m_loudness.SAMPLES_NEEDED)
  {
    const float amp_db = m_loudness.GetAmplitudeDb();
    m_loudness_level = static_cast<u16>((amp_db - m_loudness.DB_MIN) / UNIT);

#ifdef WII_SPEAK_LOG_STATS
    m_loudness.LogStats();
#else
    DEBUG_LOG_FMT(IOS_USB,
                  "Wii Speak loudness stats (sample count: {}/{}):\n"
                  " - min={} max={} amplitude={} dB\n"
                  " - level={:04x}",
                  m_loudness.samples_count, m_loudness.SAMPLES_NEEDED, m_loudness.peak_min,
                  m_loudness.peak_max, amp_db, m_loudness_level);
#endif

    m_loudness.Reset();
  }
}

bool Microphone::HasData(u32 sample_count = BUFF_SIZE_SAMPLES) const
{
  return m_samples_avail >= sample_count;
}

const WiiSpeakState& Microphone::GetSampler() const
{
  return m_sampler;
}

Microphone::FloatType Microphone::ComputeGain(FloatType relative_db) const
{
  return m_loudness.ComputeGain(relative_db);
}

const Microphone::FloatType Microphone::Loudness::DB_MIN =
    20 * std::log10(FloatType(1) / MAX_AMPLTIUDE);
const Microphone::FloatType Microphone::Loudness::DB_MAX = 20 * std::log10(FloatType(1));

Microphone::Loudness::SampleType Microphone::Loudness::GetPeak() const
{
  return std::max(std::abs(peak_min), std::abs(peak_max));
}

Microphone::FloatType Microphone::Loudness::GetDecibel(FloatType value) const
{
  return 20 * std::log10(value);
}

Microphone::FloatType Microphone::Loudness::GetAmplitude() const
{
  return GetPeak() / MAX_AMPLTIUDE;
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

Microphone::FloatType Microphone::Loudness::ComputeGain(FloatType db) const
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
               "Wii Speak loudness stats (sample count: {}/{}):\n"
               " - min={} max={} amplitude={} ({} dB)\n"
               " - rms={} ({} dB) \n"
               " - abs_mean={} ({} dB)\n"
               " - crest_factor={} ({} dB)",
               samples_count, SAMPLES_NEEDED, peak_min, peak_max, amplitude, amplitude_db, rms,
               rms_db, abs_mean, abs_mean_db, crest_factor, crest_factor_db);
}
}  // namespace IOS::HLE::USB
