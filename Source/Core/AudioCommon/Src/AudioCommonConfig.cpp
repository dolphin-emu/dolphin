// Copyright (C) 2003-2009 Dolphin Project.

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
	file.Get("Config", "EnableDTKMusic", &m_EnableDTKMusic, true);
	file.Get("Config", "EnableThrottle", &m_EnableThrottle, true);
	file.Get("Config", "Volume", &m_Volume, 0);
#ifdef _WIN32
	file.Get("Config", "Backend", &sBackend, "DSound");
#elif defined(__APPLE__)
	std::string temp;
	file.Get("Config", "Backend", &temp, "AOSound");
	strncpy(sBackend, temp.c_str(), 128);
#else
	file.Get("Config", "Backend", &sBackend, "AOSound");
#endif
}

// Set the values for the file
void AudioCommonConfig::Set(IniFile &file) {
	file.Set("Config", "EnableDTKMusic", m_EnableDTKMusic);
	file.Set("Config", "EnableThrottle", m_EnableThrottle);
	file.Set("Config", "Backend", sBackend);
	file.Set("Config", "Volume", m_Volume);
}

// Update according to the values (stream/mixer)
void AudioCommonConfig::Update() {
	if (soundStream) {
		soundStream->GetMixer()->SetThrottle(m_EnableThrottle);
		soundStream->GetMixer()->SetDTKMusic(m_EnableDTKMusic);
		soundStream->SetVolume(m_Volume);
	}
}
