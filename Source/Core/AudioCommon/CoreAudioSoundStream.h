// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef IPHONEOS
#include <AudioUnit/AudioUnit.h>
#endif

#include "AudioCommon/SoundStream.h"

class CoreAudioSound final : public SoundStream
{
#ifdef IPHONEOS
public:
  bool Init() override;
  bool SetRunning(bool running) override;
  void SetVolume(int volume) override;
  void SoundLoop() override;
  void Update() override;

  static bool isValid() { return true; }

private:
  AudioUnit audio_unit;
  int m_volume;

  static OSStatus callback(void* ref_con, AudioUnitRenderActionFlags* action_flags,
                           const AudioTimeStamp* timestamp, UInt32 bus_number, UInt32 number_frames,
                           AudioBufferList* io_data);
#endif
};
