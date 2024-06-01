// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/AlsaSoundStream.h"

#include <mutex>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"

AlsaSound::AlsaSound()
    : m_thread_status(ALSAThreadStatus::STOPPED), m_handle(nullptr), m_period_size(FRAME_COUNT_MIN)
{
}

AlsaSound::~AlsaSound()
{
  m_thread_status.store(ALSAThreadStatus::STOPPING);

  // Immediately lock and unlock mutex to prevent cv race.
  std::unique_lock<std::mutex>{cv_m};

  // Give the opportunity to the audio thread
  // to realize we are stopping the emulation
  cv.notify_one();
  if (thread.joinable())
    thread.join();
}

bool AlsaSound::Init()
{
  m_thread_status.store(ALSAThreadStatus::PAUSED);
  if (!AlsaInit())
  {
    m_thread_status.store(ALSAThreadStatus::STOPPED);
    return false;
  }

  thread = std::thread(&AlsaSound::SoundLoop, this);
  return true;
}

// Called on audio thread.
void AlsaSound::SoundLoop()
{
  Common::SetCurrentThreadName("Audio thread - alsa");
  while (m_thread_status.load() != ALSAThreadStatus::STOPPING)
  {
    int err;
    const snd_pcm_channel_area_t* areas;
    snd_pcm_uframes_t frames;
    snd_pcm_uframes_t offset;
    snd_pcm_uframes_t avail;
    snd_pcm_uframes_t size;
    snd_pcm_state_t state;

    while (m_thread_status.load() == ALSAThreadStatus::RUNNING)
    {
      state = snd_pcm_state(m_handle);
      switch (state)
      {
      case SND_PCM_STATE_XRUN:
        WARN_LOG_FMT(AUDIO, "Underrun");
        err = snd_pcm_prepare(m_handle);
        if (err < 0)
        {
          ERROR_LOG_FMT(AUDIO, "Underrun recovery failed: {}", snd_strerror(err));
          return;
        }
        break;
      case SND_PCM_STATE_SUSPENDED:
        while ((err = snd_pcm_resume(m_handle)) == -EAGAIN)
        {
        }
        if (err < 0)
        {
          err = snd_pcm_prepare(m_handle);
          if (err < 0)
          {
            ERROR_LOG_FMT(AUDIO, "Suspend recovery failed: {}", snd_strerror(err));
            return;
          }
        }
        break;
      default:
        break;
      }

      avail = snd_pcm_avail_update(m_handle);
      // Only write when we can fill the next period
      if (avail < m_buffer_size - m_period_size)
      {
        continue;
      }

      size = m_period_size;
      while (size > 0)
      {
        frames = size;
        err = snd_pcm_mmap_begin(m_handle, &areas, &offset, &frames);
        if (err == -EPIPE)
        {
          WARN_LOG_FMT(AUDIO, "Underrun");
          err = snd_pcm_prepare(m_handle);
          if (err < 0)
          {
            ERROR_LOG_FMT(AUDIO, "Prepare error: {}", snd_strerror(err));
            return;
          }
          break;
        }
        else if (err < 0)
        {
          ERROR_LOG_FMT(AUDIO, "Mmap begin error: {}", snd_strerror(err));
          break;
        }

        if (m_stereo)
        {
          s16* buf;
          buf = reinterpret_cast<s16*>(areas[0].addr) + offset * 2;
          m_mixer->Mix(buf, frames);
        }
        else
        {
          float* buf;
          buf = reinterpret_cast<float*>(areas[0].addr) + offset * 6;
          m_mixer->MixSurround(buf, frames);
        }

        err = snd_pcm_mmap_commit(m_handle, offset, frames);
        if (err == -EPIPE)
        {
          WARN_LOG_FMT(AUDIO, "Underrun");
          err = snd_pcm_prepare(m_handle);
          if (err < 0)
          {
            ERROR_LOG_FMT(AUDIO, "Prepare error: {}", snd_strerror(err));
            return;
          }
        }
        else if (err < 0)
        {
          ERROR_LOG_FMT(AUDIO, "Mmap commit error: {}", snd_strerror(err));
          return;
        }
        if (static_cast<snd_pcm_uframes_t>(err) != frames)
        {
          WARN_LOG_FMT(AUDIO, "Short write ({}/{})", err, frames);
        }

        size -= frames;
      }

      state = snd_pcm_state(m_handle);
      if (state == SND_PCM_STATE_SETUP || state == SND_PCM_STATE_PREPARED)
      {
        err = snd_pcm_start(m_handle);
        if (err < 0)
        {
          ERROR_LOG_FMT(AUDIO, "Could not start PCM: {}", snd_strerror(err));
          return;
        }
      }
    }
    if (m_thread_status.load() == ALSAThreadStatus::PAUSED)
    {
      // Stop sound output
      snd_pcm_drop(m_handle);

      // Block until thread status changes.
      std::unique_lock<std::mutex> lock(cv_m);
      cv.wait(lock, [this] { return m_thread_status.load() != ALSAThreadStatus::PAUSED; });

      // Resume sound output
      snd_pcm_prepare(m_handle);
    }
  }
  AlsaShutdown();
  m_thread_status.store(ALSAThreadStatus::STOPPED);
}

bool AlsaSound::SetRunning(bool running)
{
  m_thread_status.store(running ? ALSAThreadStatus::RUNNING : ALSAThreadStatus::PAUSED);

  // Immediately lock and unlock mutex to prevent cv race.
  std::unique_lock<std::mutex>{cv_m};

  // Notify thread that status has changed
  cv.notify_one();
  return true;
}

bool AlsaSound::AlsaInit()
{
  int err;
  int dir;
  snd_pcm_hw_params_t* hw_params;
  const char* device;
  snd_pcm_format_t format;
  unsigned int channels;
  unsigned int sample_rate;
  unsigned int periods;

  m_period_size = FRAME_COUNT_MIN;
  sample_rate = m_mixer->GetSampleRate();

  m_stereo = !Config::ShouldUseDPL2Decoder();
  channels = m_stereo ? 2 : 6;
  format = m_stereo ? SND_PCM_FORMAT_S16 : SND_PCM_FORMAT_FLOAT;
  // Enable automatic format conversion for surround since soundcards don't
  // usually support float format
  device = m_stereo ? "default" : "plug:default";

  err = snd_pcm_open(&m_handle, device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Audio open error: {}", snd_strerror(err));
    return false;
  }

  snd_pcm_hw_params_alloca(&hw_params);

  err = snd_pcm_hw_params_any(m_handle, hw_params);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Broken configuration for this PCM: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_set_access(m_handle, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Access type not available: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_set_format(m_handle, hw_params, format);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Sample format not available: {}", snd_strerror(err));
    return false;
  }

  dir = 0;
  err = snd_pcm_hw_params_set_rate(m_handle, hw_params, sample_rate, dir);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Rate not available: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_set_channels(m_handle, hw_params, channels);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Channel count not available: {}", snd_strerror(err));
    return false;
  }

  dir = 0;
  err = snd_pcm_hw_params_set_period_size_near(m_handle, hw_params, &m_period_size, &dir);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Cannot set period size: {}", snd_strerror(err));
    return false;
  }

  dir = 0;
  err = snd_pcm_hw_params_set_periods_last(m_handle, hw_params, &periods, &dir);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Cannot set period count: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params(m_handle, hw_params);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Unable to install hw params: {}", snd_strerror(err));
    return false;
  }

  m_buffer_size = periods * m_period_size;

  NOTICE_LOG_FMT(AUDIO,
                 "ALSA gave us a {} sample \"hardware\" buffer with {} periods. Will send {} "
                 "samples per fragments.",
                 m_buffer_size, periods, m_period_size);

  err = snd_pcm_prepare(m_handle);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Unable to prepare: {}", snd_strerror(err));
    return false;
  }
  NOTICE_LOG_FMT(AUDIO, "ALSA successfully initialized.");
  return true;
}

void AlsaSound::AlsaShutdown()
{
  if (m_handle != nullptr)
  {
    snd_pcm_drop(m_handle);
    snd_pcm_close(m_handle);
    m_handle = nullptr;
  }
}
