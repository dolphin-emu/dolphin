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

  // cubeb always accepts 6 channels and then downmixes to 2 if they are not natively supported
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

  // In samples. Max supported by cubeb is 96000 and min is 1 (in samples)
  u32 minimum_latency = 0;
  if (cubeb_get_min_latency(m_ctx.get(), &params, &minimum_latency) != CUBEB_OK)
    ERROR_LOG_FMT(AUDIO, "Error getting minimum latency");

  u32 final_latency = minimum_latency;
  if (SupportsCustomLatency())
  {
    const u32 target_latency = AudioCommon::GetUserTargetLatency() / 1000.0 * params.rate;
    final_latency = std::clamp(target_latency, minimum_latency, 96000u);
  }
  INFO_LOG_FMT(AUDIO, "Latency: {} frames", final_latency);

  // Note that cubeb latency might not be fixed and could dynamically adjust when the audio thread
  // can't keep up with itself, especially when using its internal mixer.
  if (cubeb_stream_init(m_ctx.get(), &m_stream, "Dolphin Audio Output", nullptr, nullptr, nullptr,
                        &params, final_latency, DataCallback, StateCallback, this) == CUBEB_OK)
  {
    cubeb_stream_set_volume(m_stream, m_volume);
    return true;
  }
  return false;
}

void CubebStream::DestroyStream()
{
  if (m_stream)
  {
    // Can't fail
    cubeb_stream_destroy(m_stream);
    m_stream = nullptr;
  }
}

bool CubebStream::SetRunning(bool running)
{
  assert(running != m_running);  // Can't happen, but if it did, it's not fully supported

  m_should_restart = false;
  if (m_settings_changed && running)
  {
    DestroyStream();
    // It's very hard for cubeb to fail starting a stream so we don't trigger a restart request
    if (!CreateStream())
    {
      return false;
    }
    m_settings_changed = false;
  }

  if (running)
  {
    if (cubeb_stream_start(m_stream) == CUBEB_OK)
    {
      m_running = true;
      return true;
    }
  }
  else
  {
    if (!m_stream || cubeb_stream_stop(m_stream) == CUBEB_OK)
    {
      m_running = false;
      return true;
    }
  }
  return false;
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

AudioCommon::SurroundState CubebStream::GetSurroundState() const
{
  if (m_stream)
  {
    return m_stereo ? AudioCommon::SurroundState::Disabled : AudioCommon::SurroundState::Enabled;
  }
  return SConfig::GetInstance().ShouldUseDPL2Decoder() ?
             AudioCommon::SurroundState::EnabledNotRunning :
             AudioCommon::SurroundState::Disabled;
}

bool CubebStream::SupportsCustomLatency()
{
#ifdef _WIN32
  // Latency is ignored in Windows, despite being exposed (always 10ms, WASAPI max is 5000ms).
  // However, it seems to somewhat be used when using its internal mixer (in case of up mixing
  // or down mixing the n of channels) and the audio thread runs too slow to keep up.
  return false;
#else
  // TODO: test whether cubeb supports latency on Mac OS X and Linux (different internal backends)
  return true;
#endif
}

void CubebStream::SetVolume(int volume)
{
  m_volume = volume / 100.f;
  cubeb_stream_set_volume(m_stream, m_volume);
}
