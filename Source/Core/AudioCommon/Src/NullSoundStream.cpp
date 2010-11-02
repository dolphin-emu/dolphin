// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "AudioCommon.h"
#include "NullSoundStream.h"

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
	// This should equal AUDIO_DMA_PERIOD.  TODO: Fix after DSP merge
	int numBytesToRender = 32000 * 4 / 32;
	m_mixer->Mix(realtimeBuffer, numBytesToRender / 4);
}

void NullSound::Clear(bool mute)
{
	m_muted = mute;
}

void NullSound::Stop()
{
}
