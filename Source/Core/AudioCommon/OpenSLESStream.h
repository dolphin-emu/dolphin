// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef HAVE_OPENSL_ES
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

  static constexpr int BUFFER_SIZE = 512;
  static constexpr int BUFFER_SIZE_IN_SAMPLES = BUFFER_SIZE / 2;

  // Double buffering.
  short m_buffer[2][BUFFER_SIZE];
  int m_current_buffer = 0;
#endif  // HAVE_OPENSL_ES
};
