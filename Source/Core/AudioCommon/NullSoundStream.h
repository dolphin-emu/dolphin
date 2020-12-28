// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/SoundStream.h"
#include "Common/Common.h"

// Sound mixer is still created and samples are still pushed to it,
// but they aren't outputtted
class NullSound final : public SoundStream
{
public:
  static std::string GetName() { return _trans("No Audio Output"); }
  bool Init() override;
  bool SetRunning(bool running) override;
};
