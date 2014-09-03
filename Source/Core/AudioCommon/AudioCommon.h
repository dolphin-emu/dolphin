// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/SoundStream.h"
#include "Common/Common.h"


class CMixer;

extern SoundStream *soundStream;

namespace AudioCommon
{
	SoundStream* InitSoundStream();
	void ShutdownSoundStream();
	std::vector<std::string> GetSoundBackends();
	std::vector<std::string> GetInterpAlgos();
	void PauseAndLock(bool doLock, bool unpauseOnUnlock=true);
	void UpdateSoundStream();
	void ClearAudioBuffer(bool mute);
	void SendAIBuffer(short* samples, unsigned int num_samples);
	float twos2float(u16 s);
	s16 float2atwos(float f);
}
