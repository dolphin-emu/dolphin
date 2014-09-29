// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/IniFile.h"

// Backend Types
#define BACKEND_NULLSOUND	"No audio output"
#define BACKEND_ALSA		"ALSA"
#define BACKEND_AOSOUND	    "AOSound"
#define BACKEND_COREAUDIO	"CoreAudio"
#define BACKEND_OPENAL		"OpenAL"
#define BACKEND_PULSEAUDIO	"Pulse"
#define BACKEND_XAUDIO2     "XAudio2"

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
