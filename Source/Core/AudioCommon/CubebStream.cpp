// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/CubebStream.h"

#include <cubeb/cubeb.h>

#include <algorithm>

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/CubebUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"

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
  if (m_ctx)
  {
    return CreateStream();
  }
  return false;
}

// Cubeb settings can be changed while the stream is ongoing so, in that case,
// we want to easily re-create it, but we didn't want to do it every time the emulation
// was paused and unpaused as it causes a small audio stutter
bool CubebStream::CreateStream()
{
  m_mixer->UpdateSettings(SConfig::GetInstance().bUseOSMixerSampleRate ?
                              AudioCommon::GetOSMixerSampleRate() :
                              AudioCommon::GetDefaultSampleRate());

  m_stereo = !SConfig::GetInstance().ShouldUseDPL2Decoder();

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

  // In samples
  // Max supported by cubeb is 96000 and min is 1 (in samples)
  u32 minimum_latency = 0;
  if (cubeb_get_min_latency(m_ctx.get(), &params, &minimum_latency) != CUBEB_OK)
    ERROR_LOG_FMT(AUDIO, "Error getting minimum latency");

#ifdef _WIN32
  // Custom latency is ignored in Windows, despite being exposed (always 10ms, WASAPI max is 5000ms)
  const u32 final_latency = minimum_latency;
#else
  const u32 target_latency = AudioCommon::GetUserTargetLatency() / 1000.0 * params.rate;
  const u32 final_latency = std::clamp(target_latency, minimum_latency, 96000u);
#endif
  INFO_LOG(AUDIO, "Latency: %u frames", final_latency);

  return cubeb_stream_init(m_ctx.get(), &m_stream, "Dolphin Audio Output", nullptr, nullptr,
                           nullptr, &params, final_latency, DataCallback, StateCallback,
                           this) == CUBEB_OK;
}

void CubebStream::DestroyStream()
{
  // Can't fail
  cubeb_stream_destroy(m_stream);
  m_stream = nullptr;
}

bool CubebStream::SetRunning(bool running)
{
  assert(running != m_running);

  m_should_restart = false;
  if (m_settings_changed && running)
  {
    m_settings_changed = false;
    DestroyStream();
    // It's very hard for cubeb to fail starting a stream so we don't trigger a restart request
    if (!CreateStream())
    {
      return false;
    }
  }

  if (running)
  {
    if (cubeb_stream_start(m_stream) == CUBEB_OK)
    {
      m_running = true;
      return true;
    }
    return false;
  }
  else
  {
    if (cubeb_stream_stop(m_stream) == CUBEB_OK)
    {
      m_running = false;
      return true;
    }
    return false;
  }
}

CubebStream::~CubebStream()
{
  if (m_running)
    SetRunning(false);
  DestroyStream();
  m_ctx.reset();
}

void CubebStream::Update()
{
  // TODO: move this out of the game (main) thread, restarting cubeb should not lock the game
  // thread, but as of now there isn't any other constant and safe access point to restart

  if (m_should_restart)
  {
    m_should_restart = false;
    if (m_running)
    {
      // We need to pass through AudioCommon as it has a mutex and
      // to make sure s_sound_stream_running is updated
      if (AudioCommon::SetSoundStreamRunning(false))
      {
        AudioCommon::SetSoundStreamRunning(true);
      }
    }
    else
    {
      AudioCommon::SetSoundStreamRunning(true);
    }
  }
}

void CubebStream::SetVolume(int volume)
{
  cubeb_stream_set_volume(m_stream, volume / 100.0f);
}
