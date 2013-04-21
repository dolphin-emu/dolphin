// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "AudioCommon.h"
#include "FileUtil.h"
#include "Mixer.h"
#include "NullSoundStream.h"
#include "DSoundStream.h"
#include "XAudio2Stream.h"
#include "AOSoundStream.h"
#include "AlsaSoundStream.h"
#include "CoreAudioSoundStream.h"
#include "OpenALStream.h"
#include "PulseAudioStream.h"
#include "OpenSLESStream.h"
#include "../../Core/Src/Movie.h"
#include "../../Core/Src/ConfigManager.h"

// This shouldn't be a global, at least not here.
CBaseSoundStream *soundStream;

namespace AudioCommon 
{	
	CBaseSoundStream *InitSoundStream(CMixer *mixer, void *hWnd) 
	{
		// TODO: possible memleak with mixer

		std::string backend = SConfig::GetInstance().sBackend;
		if (backend == BACKEND_OPENAL           && OpenALSoundStream::IsValid()) 
			soundStream = new OpenALSoundStream(mixer);
		else if (backend == BACKEND_NULLSOUND   && NullSoundStream::IsValid()) 
			soundStream = new NullSoundStream(mixer, hWnd);
		else if (backend == BACKEND_DIRECTSOUND && DSoundStream::IsValid()) 
			soundStream = new DSoundStream(mixer, hWnd);
		else if (backend == BACKEND_XAUDIO2     && XAudio2SoundStream::IsValid()) 
			soundStream = new XAudio2SoundStream(mixer);
		else if (backend == BACKEND_AOSOUND     && AOSoundStream::IsValid()) 
			soundStream = new AOSoundStream(mixer);
		else if (backend == BACKEND_ALSA        && AlsaSoundStream::IsValid())
			soundStream = new AlsaSoundStream(mixer);
		else if (backend == BACKEND_COREAUDIO   && CoreAudioSound::IsValid()) 
			soundStream = new CoreAudioSound(mixer);
		else if (backend == BACKEND_PULSEAUDIO  && PulseAudioStream::IsValid())
			soundStream = new PulseAudioStream(mixer);
		else if (backend == BACKEND_OPENSLES	&& OpenSLESSoundStream::IsValid())
			soundStream = new OpenSLESSoundStream(mixer);
		if (soundStream != NULL)
		{
			UpdateSoundStream();
			if (soundStream->StartThread())
			{
				if (SConfig::GetInstance().m_DumpAudio)
				{
					std::string audio_file_name = File::GetUserPath(D_DUMPAUDIO_IDX) + "audiodump.wav";
					File::CreateFullPath(audio_file_name);
					mixer->StartLogAudio(audio_file_name.c_str());
				}

				return soundStream;
			}
			PanicAlertT("Could not initialize backend %s.", backend.c_str());
		}
		PanicAlertT("Sound backend %s is not valid.", backend.c_str());

		delete soundStream;
		soundStream = NULL;
		return NULL;
	}

	void ShutdownSoundStream() 
	{
		INFO_LOG(DSPHLE, "Shutting down sound stream");

		if (soundStream) 
		{
			soundStream->StopThread();
			if (SConfig::GetInstance().m_DumpAudio)
				soundStream->StopRecordAudio();
			delete soundStream;
			soundStream = NULL;
		}

		INFO_LOG(DSPHLE, "Done shutting down sound stream");	
	}

	std::vector<std::string> GetSoundBackends() 
	{
		std::vector<std::string> backends;

		if (NullSoundStream::IsValid())
			backends.push_back(BACKEND_NULLSOUND);
		if (DSoundStream::IsValid())
			backends.push_back(BACKEND_DIRECTSOUND);
		if (XAudio2SoundStream::IsValid())
			backends.push_back(BACKEND_XAUDIO2);
		if (AOSoundStream::IsValid())
			backends.push_back(BACKEND_AOSOUND);
		if (AlsaSoundStream::IsValid())
			backends.push_back(BACKEND_ALSA);
		if (CoreAudioSound::IsValid())
			backends.push_back(BACKEND_COREAUDIO);
		if (PulseAudioStream::IsValid())
			backends.push_back(BACKEND_PULSEAUDIO);
		if (OpenALSoundStream::IsValid())
			backends.push_back(BACKEND_OPENAL);
		if (OpenSLESSoundStream::IsValid())
			backends.push_back(BACKEND_OPENSLES);
		return backends;
	}

	bool UseJIT() 
	{
		if (!Movie::IsDSPHLE() && Movie::IsPlayingInput() && Movie::IsConfigSaved())
		{
			return true;
		}
		return SConfig::GetInstance().m_EnableJIT;
	}

	void PauseAndLock(bool doLock, bool unpauseOnUnlock)
	{
		if (soundStream)
		{
			// audio typically doesn't maintain its own "paused" state
			// (that's already handled by the CPU and whatever else being paused)
			// so it should be good enough to only lock/unlock here.
			soundStream->SetPaused(doLock);
		}
	}
	void UpdateSoundStream()
	{
		if (soundStream)
		{
			soundStream->SetThrottle(SConfig::GetInstance().m_Framelimit == 2);
			soundStream->SetVolume(SConfig::GetInstance().m_Volume);
		}
	}
}
