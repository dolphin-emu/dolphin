// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef HAVE_OPENSL_ES
#include <array>
#include <vector>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#endif  // HAVE_OPENSL_ES

#include "AudioCommon/SoundStream.h"

class OpenSLESStream final : public SoundStream
{
#ifdef HAVE_OPENSL_ES
public:
  ~OpenSLESStream() override;
  bool Init() override;
  bool SetRunning(bool running) override;
  void SetVolume(int volume) override;
  static bool IsValid() { return true; }

private:
  static void BQPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context);
  void PushSamples(SLAndroidSimpleBufferQueueItf bq);

  // engine interfaces
  SLObjectItf m_engine_object;
  SLEngineItf m_engine_engine;
  SLObjectItf m_output_mix_object;

  // buffer queue player interfaces
  SLObjectItf m_bq_player_object = nullptr;
  SLPlayItf m_bq_player_play;
  SLAndroidSimpleBufferQueueItf m_bq_player_buffer_queue;
  SLVolumeItf m_bq_player_volume;

  SLuint32 m_frames_per_buffer;
  SLuint32 m_bytes_per_buffer;

  // Double buffering.
  std::array<std::vector<short>, 2> m_buffer;
  int m_current_buffer = 0;
#endif  // HAVE_OPENSL_ES
};
