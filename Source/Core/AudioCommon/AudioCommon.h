// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/SoundStream.h"
#include "Common/CommonTypes.h"


class CMixer;

extern SoundStream *g_sound_stream;

namespace AudioCommon
{
	SoundStream* InitSoundStream();
	void ShutdownSoundStream();
	std::vector<std::string> GetSoundBackends();
	void UpdateSoundStream();
	void ClearAudioBuffer(bool mute);
	void SendAIBuffer(short* samples, unsigned int num_samples);
	void StartAudioDump();
	void StopAudioDump();
	void IncreaseVolume(unsigned short offset);
	void DecreaseVolume(unsigned short offset);
	void ToggleMuteVolume();
}
