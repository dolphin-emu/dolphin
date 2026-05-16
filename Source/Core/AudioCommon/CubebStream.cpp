// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/CubebStream.h"

#include <cubeb/cubeb.h>

#include "AudioCommon/CubebUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/Config/MainSettings.h"
#include "Core/System.h"

#ifdef _WIN32
#include <Objbase.h>
#endif

// ~10 ms - needs to be at least 240 for surround
constexpr u32 BUFFER_SAMPLES = 512;

long CubebStream::DataCallback(cubeb_stream* stream, void* user_data, const void* /*input_buffer*/,
                               void* output_buffer, long num_frames)
{
  const auto* const self = static_cast<CubebStream*>(user_data);

  if (self->m_stereo)
    self->m_mixer->Mix(static_cast<short*>(output_buffer), num_frames);
  else
    self->m_mixer->MixSurround(static_cast<float*>(output_buffer), num_frames);

  return num_frames;
}

void CubebStream::StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state)
{
}

long CubebStream::WiimoteDataCallback(cubeb_stream* stream, void* user_data,
                                      const void* /*input_buffer*/, void* output_buffer,
                                      long num_frames)
{
  const auto* data = static_cast<const WiimoteStreamData*>(user_data);
  data->self->m_mixer->MixWiimoteSpeaker(data->wiimote_index, static_cast<short*>(output_buffer),
                                         num_frames);
  return num_frames;
}

void CubebStream::WiimoteStateCallback(cubeb_stream* stream, void* user_data, cubeb_state state)
{
}

CubebStream::CubebStream()
#ifdef _WIN32
    : m_work_queue("Cubeb Worker")
{
  m_work_queue.PushBlocking([this] {
    const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    m_coinit_success = result == S_OK;
    m_should_couninit = result == S_OK || result == S_FALSE;
  });
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
  m_work_queue.PushBlocking([this, &return_value] {
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

      // Create per-wiimote streams for audio routing (Wii games only, when enabled)
      if (return_value && Core::System::GetInstance().IsWii() &&
          Config::Get(Config::MAIN_WIIMOTE_AUDIO_ROUTING_ENABLED))
      {
        static const char* const WIIMOTE_STREAM_NAMES[4] = {
            "Dolphin Wiimote 1 Audio", "Dolphin Wiimote 2 Audio", "Dolphin Wiimote 3 Audio",
            "Dolphin Wiimote 4 Audio"};

        cubeb_stream_params wiimote_params{};
        wiimote_params.rate = m_mixer->GetSampleRate();
        wiimote_params.channels = 2;
        wiimote_params.format = CUBEB_SAMPLE_S16NE;
        wiimote_params.layout = CUBEB_LAYOUT_STEREO;

        for (std::size_t i = 0; i < m_wiimote_streams.size(); ++i)
        {
          if (!Config::Get(Config::MAIN_WIIMOTE_AUDIO_OUTPUT_ENABLED[i]))
            continue;

          const std::string device_id_str =
              Config::Get(Config::MAIN_WIIMOTE_AUDIO_OUTPUT_DEVICE[i]);
          const cubeb_devid output_devid =
              device_id_str.empty() ?
                  nullptr :
                  static_cast<cubeb_devid>(CubebUtils::GetOutputDeviceById(device_id_str));

          u32 wiimote_min_latency = 0;
          cubeb_get_min_latency(m_ctx.get(), &wiimote_params, &wiimote_min_latency);

          m_wiimote_stream_data[i] = {this, i};
          const int result = cubeb_stream_init(
              m_ctx.get(), &m_wiimote_streams[i], WIIMOTE_STREAM_NAMES[i], nullptr, nullptr,
              output_devid, &wiimote_params, std::max(BUFFER_SAMPLES, wiimote_min_latency),
              WiimoteDataCallback, WiimoteStateCallback, &m_wiimote_stream_data[i]);

          if (result != CUBEB_OK)
          {
            ERROR_LOG_FMT(AUDIO, "Failed to create Cubeb stream for Wiimote {} audio routing",
                          i + 1);
            m_wiimote_streams[i] = nullptr;
          }
        }
      }
    }

#ifdef _WIN32
  });
#endif

  return return_value;
}

bool CubebStream::SetRunning(bool running)
{
  bool return_value = false;

#ifdef _WIN32
  if (!m_coinit_success)
    return false;
  m_work_queue.PushBlocking([this, running, &return_value] {
#endif
    if (running)
    {
      return_value = cubeb_stream_start(m_stream) == CUBEB_OK;
      for (auto& ws : m_wiimote_streams)
      {
        if (ws)
          cubeb_stream_start(ws);
      }
    }
    else
    {
      return_value = cubeb_stream_stop(m_stream) == CUBEB_OK;
      for (auto& ws : m_wiimote_streams)
      {
        if (ws)
          cubeb_stream_stop(ws);
      }
    }
#ifdef _WIN32
  });
#endif

  return return_value;
}

CubebStream::~CubebStream()
{
#ifdef _WIN32
  m_work_queue.PushBlocking([this] {
#endif
    cubeb_stream_stop(m_stream);
    cubeb_stream_destroy(m_stream);
    for (auto& ws : m_wiimote_streams)
    {
      if (ws)
      {
        cubeb_stream_stop(ws);
        cubeb_stream_destroy(ws);
        ws = nullptr;
      }
    }
#ifdef _WIN32
    if (m_should_couninit)
    {
      m_should_couninit = false;
      CoUninitialize();
    }
    m_coinit_success = false;
  });
#endif
  m_ctx.reset();
}

void CubebStream::SetVolume(int volume)
{
#ifdef _WIN32
  if (!m_coinit_success)
    return;
  m_work_queue.PushBlocking([this, volume] {
#endif
    cubeb_stream_set_volume(m_stream, volume / 100.0f);
#ifdef _WIN32
  });
#endif
}
