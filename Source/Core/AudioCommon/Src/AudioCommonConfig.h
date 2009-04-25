#ifndef _AUDIO_COMMON_CONFIG_H_
#define _AUDIO_COMMON_CONFIG_H_

#include <string>
#include "IniFile.h"

// Backend Types
#define BACKEND_DIRECTSOUND "DSound"
#define BACKEND_AOSOUND     "AOSound"
#define BACKEND_OPENAL      "OpenAL"
#define BACKEND_NULL        "NullSound"
struct AudioCommonConfig 
{
	bool m_EnableDTKMusic;
    bool m_EnableThrottle;
#ifdef __APPLE__
	char sBackend[128];
#else
	std::string sBackend;
#endif
	
	// Load from given file
	void Load(IniFile &file);
	
	// Set the values for the file
	void Set(IniFile &file);

	// Update according to the values (stream/mixer)
	void Update();
};

#endif //AUDIO_COMMON_CONFIG
