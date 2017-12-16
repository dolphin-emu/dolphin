// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/SoundStream.h"

class NullSound final : public SoundStream
{
public:
  bool Start() override;
  void SoundLoop() override;
  void SetVolume(int volume) override;
  void Stop() override;
  void Update() override;

  static bool isValid() { return true; }
};
