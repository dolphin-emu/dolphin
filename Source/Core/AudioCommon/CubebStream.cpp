// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/CubebStream.h"

#include <cubeb/cubeb.h>

#include "AudioCommon/CubebUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"

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

bool CubebStream::Init()
{
  m_ctx = CubebUtils::GetContext();
  if (!m_ctx)
    return false;

  m_stereo = !Config::ShouldUseDPL2Decoder();

  cubeb_stream_params params;
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

  return cubeb_stream_init(m_ctx.get(), &m_stream, "Dolphin Audio Output", nullptr, nullptr,
                           nullptr, &params, std::max(BUFFER_SAMPLES, minimum_latency),
                           DataCallback, StateCallback, this) == CUBEB_OK;
}

bool CubebStream::SetRunning(bool running)
{
  if (running)
    return cubeb_stream_start(m_stream) == CUBEB_OK;
  else
    return cubeb_stream_stop(m_stream) == CUBEB_OK;
}

CubebStream::~CubebStream()
{
  SetRunning(false);
  cubeb_stream_destroy(m_stream);
  m_ctx.reset();
}

void CubebStream::SetVolume(int volume)
{
  cubeb_stream_set_volume(m_stream, volume / 100.0f);
}
