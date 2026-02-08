// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/NullSoundStream.h"

bool NullSound::Init()
{
  // Make Mixer aware that audio output is disabled.
  GetMixer()->SetSampleRate(0);

  return true;
}

bool NullSound::SetRunning(bool running)
{
  return true;
}

void NullSound::SetVolume(int volume)
{
}
