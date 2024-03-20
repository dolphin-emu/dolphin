// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#if defined(HAVE_ALSA) && HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

#include "AudioCommon/SoundStream.h"
#include "Common/CommonTypes.h"

class AlsaSound final : public SoundStream
{
#if defined(HAVE_ALSA) && HAVE_ALSA
public:
  AlsaSound();
  ~AlsaSound() override;

  bool Init() override;
  bool SetRunning(bool running) override;

  static bool IsValid() { return true; }

private:
  void SoundLoop();

  // target number of frames to deliver in one transfer
  static constexpr u32 FRAME_COUNT_MIN = 96;

  enum class ALSAThreadStatus
  {
    RUNNING,
    PAUSED,
    STOPPING,
    STOPPED,
  };

  bool AlsaInit();
  void AlsaShutdown();

  std::thread thread;
  std::atomic<ALSAThreadStatus> m_thread_status;
  std::condition_variable cv;
  std::mutex cv_m;

  snd_pcm_t* m_handle;
  bool m_stereo;
  snd_pcm_uframes_t m_period_size;
  snd_pcm_uframes_t m_buffer_size;
#endif
};
