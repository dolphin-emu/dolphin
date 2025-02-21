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
  std::unique_lock<std::mutex>{cv_m}.unlock();

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
      m_mixer->Mix(mix_buffer, frames_to_deliver);
      int rc = snd_pcm_writei(handle, mix_buffer, frames_to_deliver);
      if (rc == -EPIPE)
      {
        // Underrun
        snd_pcm_prepare(handle);
      }
      else if (rc < 0)
      {
        ERROR_LOG_FMT(AUDIO, "writei fail: {}", snd_strerror(rc));
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
  std::unique_lock<std::mutex>{cv_m}.unlock();

  // Notify thread that status has changed
  cv.notify_one();
  return true;
}

bool AlsaSound::AlsaInit()
{
  unsigned int sample_rate = m_mixer->GetSampleRate();
  int err;
  int dir;
  snd_pcm_sw_params_t* swparams;
  snd_pcm_hw_params_t* hwparams;
  snd_pcm_uframes_t buffer_size, buffer_size_max;
  unsigned int periods;

  err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Audio open error: {}", snd_strerror(err));
    return false;
  }

  snd_pcm_hw_params_alloca(&hwparams);

  err = snd_pcm_hw_params_any(handle, hwparams);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Broken configuration for this PCM: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Access type not available: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_set_format(handle, hwparams, SND_PCM_FORMAT_S16_LE);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Sample format not available: {}", snd_strerror(err));
    return false;
  }

  dir = 0;
  err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &sample_rate, &dir);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Rate not available: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_set_channels(handle, hwparams, CHANNEL_COUNT);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Channels count not available: {}", snd_strerror(err));
    return false;
  }

  periods = BUFFER_SIZE_MAX / FRAME_COUNT_MIN;
  err = snd_pcm_hw_params_set_periods_max(handle, hwparams, &periods, &dir);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Cannot set maximum periods per buffer: {}", snd_strerror(err));
    return false;
  }

  buffer_size_max = BUFFER_SIZE_MAX;
  err = snd_pcm_hw_params_set_buffer_size_max(handle, hwparams, &buffer_size_max);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Cannot set maximum buffer size: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params(handle, hwparams);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Unable to install hw params: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Cannot get buffer size: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_hw_params_get_periods_max(hwparams, &periods, &dir);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "Cannot get periods: {}", snd_strerror(err));
    return false;
  }

  // periods is the number of fragments alsa can wait for during one
  // buffer_size
  frames_to_deliver = buffer_size / periods;
  // limit the minimum size. pulseaudio advertises a minimum of 32 samples.
  if (frames_to_deliver < FRAME_COUNT_MIN)
    frames_to_deliver = FRAME_COUNT_MIN;
  // it is probably a bad idea to try to send more than one buffer of data
  if ((unsigned int)frames_to_deliver > buffer_size)
    frames_to_deliver = buffer_size;
  NOTICE_LOG_FMT(AUDIO,
                 "ALSA gave us a {} sample \"hardware\" buffer with {} periods. Will send {} "
                 "samples per fragments.",
                 buffer_size, periods, frames_to_deliver);

  snd_pcm_sw_params_alloca(&swparams);

  err = snd_pcm_sw_params_current(handle, swparams);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "cannot init sw params: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 0U);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "cannot set start thresh: {}", snd_strerror(err));
    return false;
  }

  err = snd_pcm_sw_params(handle, swparams);
  if (err < 0)
  {
    ERROR_LOG_FMT(AUDIO, "cannot set sw params: {}", snd_strerror(err));
    return false;
  }

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
