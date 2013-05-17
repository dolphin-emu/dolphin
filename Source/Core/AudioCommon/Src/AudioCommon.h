// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _AUDIO_COMMON_H_
#define _AUDIO_COMMON_H_

#include "Common.h"
#include "SoundStream.h"


class CMixer;

extern SoundStream *soundStream;

// UDSPControl
union UDSPControl
{
	u16 Hex;
	struct
	{
		u16 DSPReset       : 1; // Write 1 to reset and waits for 0
		u16 DSPAssertInt   : 1;
		u16 DSPHalt        : 1;

		u16 AI             : 1;
		u16 AI_mask        : 1;
		u16 ARAM           : 1;
		u16 ARAM_mask      : 1;
		u16 DSP            : 1;
		u16 DSP_mask       : 1;

		u16 ARAM_DMAState  : 1; // DSPGetDMAStatus() uses this flag
		u16 DSPInitCode    : 1;
		u16 DSPInit        : 1; // DSPInit() writes to this flag
		u16 pad            : 4;
	};
	UDSPControl(u16 _Hex = 0) : Hex(_Hex) {}
};

namespace AudioCommon 
{
	SoundStream *InitSoundStream(CMixer *mixer, void *hWnd);
	void ShutdownSoundStream();
	std::vector<std::string> GetSoundBackends();
	bool UseJIT();
	void PauseAndLock(bool doLock, bool unpauseOnUnlock=true);
	void UpdateSoundStream();
}

#endif // _AUDIO_COMMON_H_
