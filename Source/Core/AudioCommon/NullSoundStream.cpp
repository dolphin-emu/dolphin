// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/NullSoundStream.h"

void NullSound::SoundLoop()
{
}

bool NullSound::Start()
{
  return true;
}

void NullSound::SetVolume(int volume)
{
}

void NullSound::Update()
{
}

void NullSound::Stop()
{
}
