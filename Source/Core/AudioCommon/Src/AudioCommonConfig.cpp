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
#include "CommonPaths.h"
#include "FileUtil.h"

AudioCommonConfig ac_Config;

// This shouldn't be a global, at least not here.
SoundStream *soundStream;

// Load from given file
void AudioCommonConfig::Load()
{
	IniFile file;
	file.Load(std::string(File::GetUserPath(F_DSPCONFIG_IDX)).c_str());

	file.Get("Config", "EnableDTKMusic", &m_EnableDTKMusic, true);
	file.Get("Config", "EnableThrottle", &m_EnableThrottle, true);
	file.Get("Config", "EnableJIT", &m_EnableJIT, true);
#if defined __linux__ && HAVE_ALSA
	file.Get("Config", "Backend", &sBackend, BACKEND_ALSA);
#elif defined __APPLE__
	file.Get("Config", "Backend", &sBackend, BACKEND_COREAUDIO);
#elif defined _WIN32
	file.Get("Config", "Backend", &sBackend, BACKEND_DIRECTSOUND);
#else
	file.Get("Config", "Backend", &sBackend, BACKEND_NULLSOUND);
#endif
	file.Get("Config", "Frequency", &sFrequency, "48,000 Hz");
	file.Get("Config", "Volume", &m_Volume, 100);
}

// Set the values for the file
void AudioCommonConfig::SaveSettings()
{
	IniFile file;
	file.Load(std::string(File::GetUserPath(F_DSPCONFIG_IDX)).c_str());

	file.Set("Config", "EnableDTKMusic", m_EnableDTKMusic);
	file.Set("Config", "EnableThrottle", m_EnableThrottle);
	file.Set("Config", "EnableJIT", m_EnableJIT);
	file.Set("Config", "Backend", sBackend);
	file.Set("Config", "Frequency", sFrequency);
	file.Set("Config", "Volume", m_Volume);

	file.Save((std::string(File::GetUserPath(F_DSPCONFIG_IDX)).c_str()));
}

// Update according to the values (stream/mixer)
void AudioCommonConfig::Update() {
	if (soundStream) {
		soundStream->GetMixer()->SetThrottle(m_EnableThrottle);
		soundStream->GetMixer()->SetDTKMusic(m_EnableDTKMusic);
		soundStream->SetVolume(m_Volume);
	}
}
