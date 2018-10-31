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

#ifdef _WIN32
#include <OpenAL/include/al.h>
#include <OpenAL/include/alc.h>
#include <OpenAL/include/alext.h>

// OpenAL requires a minimum of two buffers, three or more recommended
#define OAL_BUFFERS 3
#define OAL_MAX_FRAMES 4096
#define STEREO_CHANNELS 2
#define SURROUND_CHANNELS 6  // number of channels in surround mode
#define SIZE_SHORT 2
#define SIZE_INT32 4
#define SIZE_FLOAT 4  // size of a float in bytes
#define FRAME_STEREO_SHORT STEREO_CHANNELS* SIZE_SHORT
#define FRAME_SURROUND_FLOAT SURROUND_CHANNELS* SIZE_FLOAT
#define FRAME_SURROUND_SHORT SURROUND_CHANNELS* SIZE_SHORT
#define FRAME_SURROUND_INT32 SURROUND_CHANNELS* SIZE_INT32
#endif  // _WIN32

// From AL_EXT_float32
#ifndef AL_FORMAT_STEREO_FLOAT32
#define AL_FORMAT_STEREO_FLOAT32 0x10011
#endif

// From AL_EXT_MCFORMATS
#ifndef AL_FORMAT_51CHN16
#define AL_FORMAT_51CHN16 0x120B
#endif
#ifndef AL_FORMAT_51CHN32
#define AL_FORMAT_51CHN32 0x120C
#endif

// Only X-Fi on Windows supports the alext AL_FORMAT_STEREO32 alext for now,
// but it is not documented or in "OpenAL/include/al.h".
#ifndef AL_FORMAT_STEREO32
#define AL_FORMAT_STEREO32 0x1203
#endif

class OpenALStream final : public SoundStream
{
#ifdef _WIN32
public:
  OpenALStream() : m_source(0) {}
  ~OpenALStream() override;
  bool Init() override;
  void SoundLoop() override;
  void SetVolume(int volume) override;
  bool SetRunning(bool running) override;
  void Update() override;

  static bool isValid();

private:
  std::thread m_thread;
  Common::Flag m_run_thread;

  Common::Event m_sound_sync_event;

  std::vector<short> m_realtime_buffer;
  std::array<ALuint, OAL_BUFFERS> m_buffers;
  ALuint m_source;
  ALfloat m_volume;

#endif  // _WIN32
};
