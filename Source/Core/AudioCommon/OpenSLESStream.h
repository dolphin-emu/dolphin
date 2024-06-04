// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <thread>

#include "AudioCommon/SoundStream.h"
#include "Common/Event.h"

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
  std::thread thread;
  Common::Event soundSyncEvent;
#endif  // HAVE_OPENSL_ES
};
