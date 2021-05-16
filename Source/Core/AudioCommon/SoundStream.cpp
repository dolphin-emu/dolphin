// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/SoundStream.h"
#include "AudioCommon/AudioCommon.h"

SoundStream::SoundStream() : m_mixer(new Mixer(AudioCommon::GetDefaultSampleRate()))
{
}
