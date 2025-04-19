// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cubeb/cubeb.h>

#include "AudioCommon/CubebUtils.h"
#include "Common/Logging/Log.h"

#include "Common/ScopeGuard.h"
#include "Common/Swap.h"
#include "Core/IOS/USB/Emulated/Microphone.h"

#include <algorithm>
#include <mutex>

#ifdef _WIN32
#include <Objbase.h>
#endif

namespace IOS::HLE::USB
{
Microphone::Microphone()
{
  StreamInit();
}

Microphone::~Microphone()
{
  StreamTerminate();

#ifdef _WIN32
  if (m_should_couninit)
  {
    Common::Event sync_event;
    m_work_queue.EmplaceItem([this, &sync_event] {
      Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
      m_should_couninit = false;
      CoUninitialize();
    });
    sync_event.Wait();
  }
  m_coinit_success = false;
#endif
}

void Microphone::StreamInit()
{
#ifdef _WIN32
  if (!m_coinit_success)
    return;
  Common::Event sync_event;
  m_work_queue.EmplaceItem([this, &sync_event] {
    Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
    m_cubeb_ctx = CubebUtils::GetContext();
#ifdef _WIN32
  });
  sync_event.Wait();
#endif

  // TODO: Not here but rather inside the WiiSpeak device if possible?
  StreamStart();
}

void Microphone::StreamTerminate()
{
  StopStream();

  if (m_cubeb_ctx)
  {
#ifdef _WIN32
    if (!m_coinit_success)
      return;
    Common::Event sync_event;
    m_work_queue.EmplaceItem([this, &sync_event] {
      Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
      m_cubeb_ctx.reset();
#ifdef _WIN32
    });
    sync_event.Wait();
#endif
  }
}

static void state_callback(cubeb_stream* stream, void* user_data, cubeb_state state)
{
}

void Microphone::StreamStart()
{
  if (!m_cubeb_ctx)
    return;

#ifdef _WIN32
  if (!m_coinit_success)
    return;
  Common::Event sync_event;
  m_work_queue.EmplaceItem([this, &sync_event] {
    Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
    stream_size = buff_size_samples * 500;
    stream_buffer = new s16[stream_size];

    cubeb_stream_params params{};
    params.format = CUBEB_SAMPLE_S16LE;
    params.rate = SAMPLING_RATE;
    params.channels = 1;
    params.layout = CUBEB_LAYOUT_MONO;

    u32 minimum_latency;
    if (cubeb_get_min_latency(m_cubeb_ctx.get(), &params, &minimum_latency) != CUBEB_OK)
    {
      WARN_LOG_FMT(EXPANSIONINTERFACE, "Error getting minimum latency");
    }

    if (cubeb_stream_init(m_cubeb_ctx.get(), &m_cubeb_stream,
                          "Dolphin Emulated GameCube Microphone", nullptr, &params, nullptr,
                          nullptr, std::max<u32>(16, minimum_latency), DataCallback, state_callback,
                          this) != CUBEB_OK)
    {
      ERROR_LOG_FMT(IOS_USB, "Error initializing cubeb stream");
      return;
    }

    if (cubeb_stream_start(m_cubeb_stream) != CUBEB_OK)
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "Error starting cubeb stream");
      return;
    }

    INFO_LOG_FMT(EXPANSIONINTERFACE, "started cubeb stream");
#ifdef _WIN32
  });
  sync_event.Wait();
#endif
}

void Microphone::StopStream()
{
  if (m_cubeb_stream)
  {
#ifdef _WIN32
    Common::Event sync_event;
    m_work_queue.EmplaceItem([this, &sync_event] {
      Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
      if (cubeb_stream_stop(m_cubeb_stream) != CUBEB_OK)
        ERROR_LOG_FMT(IOS_USB, "Error stopping cubeb stream");
      cubeb_stream_destroy(m_cubeb_stream);
      m_cubeb_stream = nullptr;
#ifdef _WIN32
    });
    sync_event.Wait();
#endif
  }
}

long Microphone::DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                              void* /*output_buffer*/, long nframes)
{
  auto* mic = static_cast<Microphone*>(user_data);

  std::lock_guard lk(mic->ring_lock);

  const s16* buff_in = static_cast<const s16*>(input_buffer);
  for (long i = 0; i < nframes; i++)
  {
    mic->stream_buffer[mic->stream_wpos] = Common::swap16(buff_in[i]);
    mic->stream_wpos = (mic->stream_wpos + 1) % mic->stream_size;
  }

  mic->samples_avail += nframes;
  if (mic->samples_avail > mic->stream_size)
  {
    mic->samples_avail = 0;
  }

  return nframes;
}

void Microphone::ReadIntoBuffer(u8* dst, u32 size)
{
  std::lock_guard lk(ring_lock);

  if (samples_avail >= buff_size_samples)
  {
    u8* last_buffer = reinterpret_cast<u8*>(&stream_buffer[stream_rpos]);
    std::memcpy(dst, static_cast<u8*>(last_buffer), size);

    samples_avail -= buff_size_samples;

    stream_rpos += buff_size_samples;
    stream_rpos %= stream_size;
  }
}

bool Microphone::HasData() const
{
  return samples_avail > 0;
}
}  // namespace IOS::HLE::USB
