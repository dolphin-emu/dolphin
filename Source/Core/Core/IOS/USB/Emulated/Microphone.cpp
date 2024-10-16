// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Microphone.h"

#include <algorithm>

#include <cubeb/cubeb.h>

#include "AudioCommon/CubebUtils.h"
#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
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

  auto* mic = static_cast<Microphone*>(user_data);
  const auto& sampler = mic->GetSampler();

  // Skip data if sampling is off or mute is on
  if (sampler.sample_on == false || sampler.mute == true)
    return nframes;

  std::lock_guard lk(mic->m_ring_lock);

  const s16* buff_in = static_cast<const s16*>(input_buffer);
  for (long i = 0; i < nframes; i++)
  {
    mic->m_stream_buffer[mic->m_stream_wpos] = Common::swap16(buff_in[i]);
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

bool Microphone::HasData() const
{
  return m_samples_avail > 0 && !Config::Get(Config::MAIN_WII_SPEAK_MUTED);
}

const WiiSpeakState& Microphone::GetSampler() const
{
  return m_sampler;
}
}  // namespace IOS::HLE::USB
