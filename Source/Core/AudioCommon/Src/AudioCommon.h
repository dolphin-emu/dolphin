#ifndef _AUDIO_COMMON_H
#define _AUDIO_COMMON_H

#include "Common.h"
#include "../../../PluginSpecs/pluginspecs_dsp.h"
#include "SoundStream.h"

class CMixer;

extern DSPInitialize g_dspInitialize;
extern SoundStream *soundStream;

namespace AudioCommon {
	
	SoundStream *InitSoundStream(std::string backend, CMixer *mixer = NULL);
	void ShutdownSoundStream();
	 std::vector<std::string> GetSoundBackends();
} // Namespace	

#endif // AUDIO_COMMON
