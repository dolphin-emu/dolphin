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

#ifndef _AUDIO_COMMON_CONFIG_H_
#define _AUDIO_COMMON_CONFIG_H_

#include <string>
#include "Common/IniFile.h"

// Backend Types
#define BACKEND_NULLSOUND	"No audio output"
#define BACKEND_ALSA		"ALSA"
#define BACKEND_AOSOUND		"AOSound"
#define BACKEND_COREAUDIO	"CoreAudio"
#define BACKEND_DIRECTSOUND	"DSound"
#define BACKEND_OPENAL		"OpenAL"
#define BACKEND_PULSEAUDIO	"Pulse"
#define BACKEND_XAUDIO2		"XAudio2"

struct AudioCommonConfig 
{
	bool m_EnableJIT;
	bool m_DumpAudio;
	bool m_DumpAudioToAVI;
	int m_Volume;
	std::string sBackend;
	int iFrequency;
	
	// Load from given file
	void Load();
	
	// Self explanatory
	void SaveSettings();

	// Update according to the values (stream/mixer)
	void Update();
};

#endif //AUDIO_COMMON_CONFIG
