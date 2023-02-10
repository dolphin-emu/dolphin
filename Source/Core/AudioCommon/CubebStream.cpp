// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/CubebStream.h"

#include <cubeb/cubeb.h>

#include "AudioCommon/CubebUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"

#ifdef _WIN32
#include <Objbase.h>
#endif

// ~10 ms - needs to be at least 240 for surround
constexpr u32 BUFFER_SAMPLES = 512;

long CubebStream::DataCallback(cubeb_stream* stream, void* user_data, const void* /*input_buffer*/,
                               void* output_buffer, long num_frames)
{
  auto* self = static_cast<CubebStream*>(user_data);

  if (self->m_stereo)
    self->m_mixer->Mix(static_cast<short*>(output_buffer), num_frames);
  else
    self->m_mixer->MixSurround(static_cast<float*>(output_buffer), num_frames);

  return num_frames;
}

void CubebStream::StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state)
{
}

CubebStream::CubebStream()
#ifdef _WIN32
    : m_work_queue("Cubeb Worker", [](const std::function<void()>& func) { func(); })
{
  Common::Event sync_event;
  m_work_queue.EmplaceItem([this, &sync_event] {
    Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
    auto result = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    m_coinit_success = result == S_OK;
    m_should_couninit = result == S_OK || result == S_FALSE;
  });
  sync_event.Wait();
}
#else
    = default;
#endif

bool CubebStream::Init()
{
  bool return_value = false;

#ifdef _WIN32
  if (!m_coinit_success)
    return false;
  Common::Event sync_event;
  m_work_queue.EmplaceItem([this, &return_value, &sync_event] {
    Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif

    m_ctx = CubebUtils::GetContext();
    if (m_ctx)
    {
      m_stereo = !Config::ShouldUseDPL2Decoder();

      cubeb_stream_params params{};
      params.rate = m_mixer->GetSampleRate();
      if (m_stereo)
      {
        params.channels = 2;
        params.format = CUBEB_SAMPLE_S16NE;
        params.layout = CUBEB_LAYOUT_STEREO;
      }
      else
      {
        params.channels = 6;
        params.format = CUBEB_SAMPLE_FLOAT32NE;
        params.layout = CUBEB_LAYOUT_3F2_LFE;
      }

      u32 minimum_latency = 0;
      if (cubeb_get_min_latency(m_ctx.get(), &params, &minimum_latency) != CUBEB_OK)
        ERROR_LOG_FMT(AUDIO, "Error getting minimum latency");
      INFO_LOG_FMT(AUDIO, "Minimum latency: {} frames", minimum_latency);

      return_value =
          cubeb_stream_init(m_ctx.get(), &m_stream, "Dolphin Audio Output", nullptr, nullptr,
                            nullptr, &params, std::max(BUFFER_SAMPLES, minimum_latency),
                            DataCallback, StateCallback, this) == CUBEB_OK;
    }

#ifdef _WIN32
  });
  sync_event.Wait();
#endif

  return return_value;
}

bool CubebStream::SetRunning(bool running)
{
  bool return_value = false;

#ifdef _WIN32
  if (!m_coinit_success)
    return false;
  Common::Event sync_event;
  m_work_queue.EmplaceItem([this, running, &return_value, &sync_event] {
    Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
    if (running)
      return_value = cubeb_stream_start(m_stream) == CUBEB_OK;
    else
      return_value = cubeb_stream_stop(m_stream) == CUBEB_OK;
#ifdef _WIN32
  });
  sync_event.Wait();
#endif

  return return_value;
}

CubebStream::~CubebStream()
{
#ifdef _WIN32
  Common::Event sync_event;
  m_work_queue.EmplaceItem([this, &sync_event] {
    Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
    cubeb_stream_stop(m_stream);
    cubeb_stream_destroy(m_stream);
#ifdef _WIN32
    if (m_should_couninit)
    {
      m_should_couninit = false;
      CoUninitialize();
    }
    m_coinit_success = false;
  });
  sync_event.Wait();
#endif
  m_ctx.reset();
}

void CubebStream::SetVolume(int volume)
{
#ifdef _WIN32
  if (!m_coinit_success)
    return;
  Common::Event sync_event;
  m_work_queue.EmplaceItem([this, volume, &sync_event] {
    Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
    cubeb_stream_set_volume(m_stream, volume / 100.0f);
#ifdef _WIN32
  });
  sync_event.Wait();
#endif
}
