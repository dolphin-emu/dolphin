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
AudioCommonConfig ac_Config;

// Load from given file
void AudioCommonConfig::Load(IniFile &file) {
	Section& config = file["Config"];
	config.Get("EnableDTKMusic", &m_EnableDTKMusic, true);
	config.Get("EnableThrottle", &m_EnableThrottle, true);
	config.Get("EnableJIT", &m_EnableJIT, true);
	config.Get("Volume", &m_Volume, 75);
#ifdef _WIN32
	config.Get("Backend", &sBackend, BACKEND_DIRECTSOUND);
#elif defined(__APPLE__)
	std::string temp;
	config.Get("Backend", &temp, BACKEND_COREAUDIO);
	strncpy(sBackend, temp.c_str(), 128);
#else // linux
	config.Get("Backend", &sBackend, BACKEND_ALSA);
#endif
}

// Set the values for the file
void AudioCommonConfig::Set(IniFile &file) {
	Section& config = file["Config"];
	config.Set("EnableDTKMusic", m_EnableDTKMusic);
	config.Set("EnableThrottle", m_EnableThrottle);
	config.Set("EnableJIT", m_EnableJIT);
	config.Set("Backend", sBackend);
	config.Set("Volume", m_Volume);
}

// Update according to the values (stream/mixer)
void AudioCommonConfig::Update() {
	if (soundStream) {
		soundStream->GetMixer()->SetThrottle(m_EnableThrottle);
		soundStream->GetMixer()->SetDTKMusic(m_EnableDTKMusic);
		soundStream->SetVolume(m_Volume);
	}
}
