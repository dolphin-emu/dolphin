// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class SoundStream
{
public:
  SoundStream() {}
  virtual ~SoundStream() {}
  static bool isValid() { return false; }
  virtual bool Init() { return false; }
  virtual void SetVolume(int) {}
  virtual void SoundLoop() {}
  virtual void Update() {}
  virtual bool SetRunning(bool running) { return false; }
};
