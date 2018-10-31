// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <thread>

#include "AudioCommon/SoundStream.h"
#include "Common/Event.h"

class OpenSLESStream final : public SoundStream
{
#ifdef ANDROID
public:
  ~OpenSLESStream() override;
  bool Init() override;
  bool SetRunning(bool running) override { return running; }
  static bool isValid() { return true; }

private:
  std::thread thread;
  Common::Event soundSyncEvent;
#endif  // HAVE_OPENSL
};
