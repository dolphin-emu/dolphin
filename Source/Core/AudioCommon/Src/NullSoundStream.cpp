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
#include "../../Core/Src/HW/SystemTimers.h"
#include "../../Core/Src/HW/AudioInterface.h"

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
	// num_samples_to_render in this update - depends on SystemTimers::AUDIO_DMA_PERIOD.
	const u32 stereo_16_bit_size = 4;
	const u32 dma_length = 32;
	const u64 audio_dma_period = SystemTimers::GetTicksPerSecond() / (AudioInterface::GetAIDSampleRate() * stereo_16_bit_size / dma_length);
	const u64 ais_samples_per_second = 48000 * stereo_16_bit_size;
	const u64 num_samples_to_render = (audio_dma_period * ais_samples_per_second) / SystemTimers::GetTicksPerSecond();

	m_mixer->Mix(realtimeBuffer, (unsigned int)num_samples_to_render);
}

void NullSound::Clear(bool mute)
{
	m_muted = mute;
}

void NullSound::Stop()
{
}
