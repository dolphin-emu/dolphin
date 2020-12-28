// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <thread>

#include "AudioCommon/SoundStream.h"
#include "Common/Event.h"

class OpenSLESStream final : public SoundStream
{
public:
  static std::string GetName() { return "OpenSLES"; }
#ifdef ANDROID
  static bool IsValid() { return true; }
  ~OpenSLESStream() override;
  bool Init() override;
  bool SetRunning(bool running) override { return true; }
  void SetVolume(int volume) override;
  static bool SupportsVolumeChanges() { return true; }

private:
  std::thread thread;
  Common::Event soundSyncEvent;
#endif  // HAVE_OPENSL
};
