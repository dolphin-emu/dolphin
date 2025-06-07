// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <condition_variable>
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
  bool Start() override;
  bool Stop() override;

  static bool IsValid() { return true; }

private:
  void SoundLoop();

  // maximum number of frames the buffer can hold
  static constexpr size_t BUFFER_SIZE_MAX = 8192;

  // minimum number of frames to deliver in one transfer
  static constexpr u32 FRAME_COUNT_MIN = 256;

  // number of channels per frame
  static constexpr u32 CHANNEL_COUNT = 2;

  enum class ALSAThreadStatus
  {
    RUNNING,
    PAUSED,
    STOPPING,
    STOPPED,
  };

  bool AlsaInit();
  void AlsaShutdown();

  s16 mix_buffer[BUFFER_SIZE_MAX * CHANNEL_COUNT];
  std::thread thread;
  std::atomic<ALSAThreadStatus> m_thread_status;
  std::condition_variable cv;
  std::mutex cv_m;

  snd_pcm_t* handle;
  unsigned int frames_to_deliver;
#endif
};
