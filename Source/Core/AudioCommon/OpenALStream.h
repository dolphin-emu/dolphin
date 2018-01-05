// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <thread>

#include "AudioCommon/SoundStream.h"
#include "Common/Event.h"
#include "Core/Core.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/SystemTimers.h"

#if defined HAVE_OPENAL && HAVE_OPENAL
#ifdef _WIN32
#include <OpenAL/include/al.h>
#include <OpenAL/include/alc.h>
#include <OpenAL/include/alext.h>
#elif defined __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#endif

#define SFX_MAX_SOURCE 1
#define OAL_MAX_BUFFERS 32
#define OAL_MAX_SAMPLES 256
#define STEREO_CHANNELS 2
#define SURROUND_CHANNELS 6  // number of channels in surround mode
#define SIZE_SHORT 2
#define SIZE_INT32 4
#define SIZE_FLOAT 4  // size of a float in bytes
#define FRAME_STEREO_SHORT STEREO_CHANNELS* SIZE_SHORT
#define FRAME_STEREO_FLOAT STEREO_CHANNELS* SIZE_FLOAT
#define FRAME_STEREO_INT32 STEREO_CHANNELS* SIZE_INT32
#define FRAME_SURROUND_FLOAT SURROUND_CHANNELS* SIZE_FLOAT
#define FRAME_SURROUND_SHORT SURROUND_CHANNELS* SIZE_SHORT
#define FRAME_SURROUND_INT32 SURROUND_CHANNELS* SIZE_INT32
#endif

#if defined(__APPLE__)
// OS X does not have the alext AL_FORMAT_STEREO_FLOAT32, AL_FORMAT_STEREO32,
// AL_FORMAT_51CHN32 and AL_FORMAT_51CHN16 yet.
#define AL_FORMAT_STEREO_FLOAT32 0
#define AL_FORMAT_STEREO32 0
#define AL_FORMAT_51CHN32 0
#define AL_FORMAT_51CHN16 0
#elif defined(_WIN32)
// Only X-Fi on Windows supports the alext AL_FORMAT_STEREO32 alext for now,
// but it is not documented or in "OpenAL/include/al.h".
#define AL_FORMAT_STEREO32 0x1203
#else
#define AL_FORMAT_STEREO32 0
#endif

class OpenALStream final : public SoundStream
{
#if defined HAVE_OPENAL && HAVE_OPENAL
public:
  OpenALStream() : uiSource(0) {}
  bool Start() override;
  void SoundLoop() override;
  void SetVolume(int volume) override;
  void Stop() override;
  void Clear(bool mute) override;
  void Update() override;

  static bool isValid() { return true; }
private:
  std::thread thread;
  Common::Flag m_run_thread;

  Common::Event soundSyncEvent;

  short realtimeBuffer[OAL_MAX_SAMPLES * STEREO_CHANNELS];
  float sampleBuffer[OAL_MAX_SAMPLES * SURROUND_CHANNELS * OAL_MAX_BUFFERS];
  ALuint uiBuffers[OAL_MAX_BUFFERS];
  ALuint uiSource;
  ALfloat fVolume;

  u8 numBuffers;
#endif  // HAVE_OPENAL
};
