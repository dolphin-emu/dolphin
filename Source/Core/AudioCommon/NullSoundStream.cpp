// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/NullSoundStream.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/SystemTimers.h"


bool NullSound::Start()
{
	m_enablesoundloop = false;	
	return true;
}

void NullSound::SetVolume(int volume)
{
}

void NullSound::Update()
{
	m_mixer->Mix(realtimeBuffer, 0);
}

void NullSound::Clear(bool mute)
{
	m_muted = mute;
}

void NullSound::Stop()
{

}
