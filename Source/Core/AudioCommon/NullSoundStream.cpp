// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/NullSoundStream.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/SystemTimers.h"

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
