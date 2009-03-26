#ifndef _AUDIO_COMMON_H
#define _AUDIO_COMMON_H

#include "Common.h"
#include "pluginspecs_dsp.h"
#include "SoundStream.h"

class CMixer;

extern DSPInitialize g_dspInitialize;
extern SoundStream *soundStream;

namespace AudioCommon {
	
	SoundStream *InitSoundStream(std::string backend, CMixer *mixer = NULL);
} // Namespace	

#endif // AUDIO_COMMON
