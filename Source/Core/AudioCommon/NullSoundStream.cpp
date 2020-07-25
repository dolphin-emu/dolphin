// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/NullSoundStream.h"

bool NullSound::Init()
{
  return true;
}

bool NullSound::SetRunning(bool running)
{
  return true;
}
