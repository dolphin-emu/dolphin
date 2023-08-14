// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/AlsaSoundStream.h"

#include <mutex>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"

AlsaSound::AlsaSound()
    : m_thread_status(ALSAThreadStatus::STOPPED), handle(nullptr),
      frames_to_deliver(FRAME_COUNT_MIN)
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
    while (m_thread_status.load() == ALSAThreadStatus::RUNNING)
    {
      int rc;
      const snd_pcm_channel_area_t* areas;
      s16* buf;
      snd_pcm_uframes_t frames;
      snd_pcm_uframes_t offset;
      snd_pcm_uframes_t avail;
      snd_pcm_state_t state;
      avail = snd_pcm_avail(handle);
      frames = avail;
      rc = snd_pcm_mmap_begin(handle, &areas, &offset, &frames);
      if (rc == -EPIPE)
      {
        WARN_LOG_FMT(AUDIO, "Underrun");
        snd_pcm_prepare(handle);
      }
      else if (rc < 0)
      {
        WARN_LOG_FMT(AUDIO, "Mmap begin error: {}", snd_strerror(rc));
      }
      buf = reinterpret_cast<s16*>(areas[0].addr) + offset * 2;
      m_mixer->Mix(buf, frames);
      rc = snd_pcm_mmap_commit(handle, offset, frames);
      if (rc < 0)
      {
        WARN_LOG_FMT(AUDIO, "Mmap commit error: {}", snd_strerror(rc));
      }

      state = snd_pcm_state(handle);
      switch (state)
      {
      case SND_PCM_STATE_SETUP:
      case SND_PCM_STATE_PREPARED:
        snd_pcm_start(handle);
        break;
      case SND_PCM_STATE_XRUN:
        WARN_LOG_FMT(AUDIO, "Underrun");
        snd_pcm_prepare(handle);
        break;
      default:
        break;
      }
    }
    if (m_thread_status.load() == ALSAThreadStatus::PAUSED)
    {
      snd_pcm_drop(handle);  // Stop sound output

      // Block until thread status changes.
      std::unique_lock<std::mutex> lock(cv_m);
      cv.wait(lock, [this] { return m_thread_status.load() != ALSAThreadStatus::PAUSED; });

      snd_pcm_prepare(handle);  // resume sound output
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
  // snd_pcm_sw_params_t* sw_params;
  snd_pcm_hw_params_t* hw_params;
  snd_pcm_format_t format;
  unsigned int sample_rate;
  snd_pcm_uframes_t period_size;
  snd_pcm_uframes_t buffer_size;
  unsigned int periods;
  sample_rate = m_mixer->GetSampleRate();
  format = SND_PCM_FORMAT_S16;
  period_size = FRAME_COUNT_MIN;
  periods = 4;

  err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Audio open error: {}", snd_strerror(err));
    return false;
  }

  snd_pcm_hw_params_alloca(&hw_params);

  err = snd_pcm_hw_params_any(handle, hw_params);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Broken configuration for this PCM: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Access type not available: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_set_format(handle, hw_params, format);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Sample format not available: {}", snd_strerror(err));
    return false;
  }

  dir = 0;
  err = snd_pcm_hw_params_set_rate(handle, hw_params, sample_rate, dir);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Rate not available: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_set_channels(handle, hw_params, CHANNEL_COUNT);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Channel count not available: {}", snd_strerror(err));
    return false;
  }

  dir = 0;
  err = snd_pcm_hw_params_set_period_size(handle, hw_params, period_size, dir);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Cannot set maximum periods per buffer: {}", snd_strerror(err));
    return false;
  }
  buffer_size = periods * period_size;

  err = snd_pcm_hw_params_set_periods(handle, hw_params, periods, 0);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Cannot set maximum buffer size: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params(handle, hw_params);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Unable to install hw params: {}", snd_strerror(err));
    return false;
  }

  // periods is the number of fragments alsa can wait for during one
  // buffer_size
  frames_to_deliver = period_size;

  NOTICE_LOG_FMT(AUDIO,
                 "ALSA gave us a {} sample \"hardware\" buffer with {} periods. Will send {} "
                 "samples per fragments.",
                 buffer_size, periods, frames_to_deliver);

  /*
  snd_pcm_sw_params_alloca(&sw_params);

  err = snd_pcm_sw_params_current(handle, sw_params);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "cannot init sw params: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_sw_params_set_start_threshold(handle, sw_params, 0U);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "cannot set start thresh: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_sw_params(handle, sw_params);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "cannot set sw params: {}", snd_strerror(err));
    return false;
  }
  */

  err = snd_pcm_prepare(handle);
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
  if (handle != nullptr)
  {
    snd_pcm_drop(handle);
    snd_pcm_close(handle);
    handle = nullptr;
  }
}
