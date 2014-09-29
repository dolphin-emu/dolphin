// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "AudioCommon/AudioCommon.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"

AudioCommonConfig ac_Config;

// This shouldn't be a global, at least not here.
SoundStream *soundStream;

// Load from given file
void AudioCommonConfig::Load()
{
	IniFile file;
	file.Load(File::GetUserPath(F_DSPCONFIG_IDX));

	file.Get("Config", "EnableJIT", &m_EnableJIT, true);
	file.Get("Config", "DumpAudio", &m_DumpAudio, false);
	file.Get("Config", "DumpAudioToAVI", &m_DumpAudioToAVI, false);
#if defined __linux__ && HAVE_ALSA
	file.Get("Config", "Backend", &sBackend, BACKEND_ALSA);
#elif defined __APPLE__
	file.Get("Config", "Backend", &sBackend, BACKEND_COREAUDIO);
#elif defined _WIN32
	file.Get("Config", "Backend", &sBackend, BACKEND_DIRECTSOUND);
#else
	file.Get("Config", "Backend", &sBackend, BACKEND_NULLSOUND);
#endif
	file.Get("Config", "Frequency", &iFrequency, 48000);
	file.Get("Config", "Volume", &m_Volume, 100);
}

// Set the values for the file
void AudioCommonConfig::SaveSettings()
{
	IniFile file;
	file.Load(File::GetUserPath(F_DSPCONFIG_IDX));

	file.Set("Config", "EnableJIT", m_EnableJIT);
	file.Set("Config", "DumpAudio", m_DumpAudio);
	file.Set("Config", "DumpAudioToAVI", m_DumpAudioToAVI);
	file.Set("Config", "Backend", sBackend);
	file.Set("Config", "Frequency", iFrequency);
	file.Set("Config", "Volume", m_Volume);

	file.Save(File::GetUserPath(F_DSPCONFIG_IDX));
}

// Update according to the values (stream/mixer)
void AudioCommonConfig::Update() 
{
	if (soundStream) {
		soundStream->GetMixer()->SetThrottle(SConfig::GetInstance().m_Framelimit == 2);
		soundStream->SetVolume(m_Volume);
	}
}
