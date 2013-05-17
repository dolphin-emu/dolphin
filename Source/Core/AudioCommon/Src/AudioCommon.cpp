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
SoundStream *soundStream;

namespace AudioCommon 
{	
	SoundStream *InitSoundStream(CMixer *mixer, void *hWnd) 
	{
		// TODO: possible memleak with mixer

		std::string backend = SConfig::GetInstance().sBackend;
		if (backend == BACKEND_OPENAL           && OpenALStream::isValid()) 
			soundStream = new OpenALStream(mixer);
		else if (backend == BACKEND_NULLSOUND   && NullSound::isValid()) 
			soundStream = new NullSound(mixer, hWnd);
		else if (backend == BACKEND_DIRECTSOUND && DSound::isValid()) 
			soundStream = new DSound(mixer, hWnd);
		else if (backend == BACKEND_XAUDIO2     && XAudio2::isValid()) 
			soundStream = new XAudio2(mixer);
		else if (backend == BACKEND_AOSOUND     && AOSound::isValid()) 
			soundStream = new AOSound(mixer);
		else if (backend == BACKEND_ALSA        && AlsaSound::isValid())
			soundStream = new AlsaSound(mixer);
		else if (backend == BACKEND_COREAUDIO   && CoreAudioSound::isValid()) 
			soundStream = new CoreAudioSound(mixer);
		else if (backend == BACKEND_PULSEAUDIO  && PulseAudio::isValid())
			soundStream = new PulseAudio(mixer);
		else if (backend == BACKEND_OPENSLES && OpenSLESStream::isValid())
			soundStream = new OpenSLESStream(mixer);
		if (soundStream != NULL)
		{
			UpdateSoundStream();
			if (soundStream->Start())
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
			soundStream->Stop();
			if (SConfig::GetInstance().m_DumpAudio)
				soundStream->GetMixer()->StopLogAudio();
				//soundStream->StopLogAudio();
			delete soundStream;
			soundStream = NULL;
		}

		INFO_LOG(DSPHLE, "Done shutting down sound stream");	
	}

	std::vector<std::string> GetSoundBackends() 
	{
		std::vector<std::string> backends;

		if (NullSound::isValid())
			backends.push_back(BACKEND_NULLSOUND);
		if (DSound::isValid())
			backends.push_back(BACKEND_DIRECTSOUND);
		if (XAudio2::isValid())
			backends.push_back(BACKEND_XAUDIO2);
		if (AOSound::isValid())
			backends.push_back(BACKEND_AOSOUND);
		if (AlsaSound::isValid())
			backends.push_back(BACKEND_ALSA);
		if (CoreAudioSound::isValid())
			backends.push_back(BACKEND_COREAUDIO);
		if (PulseAudio::isValid())
			backends.push_back(BACKEND_PULSEAUDIO);
		if (OpenALStream::isValid())
			backends.push_back(BACKEND_OPENAL);
		if (OpenSLESStream::isValid())
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
			CMixer* pMixer = soundStream->GetMixer();
			if (pMixer)
			{
				std::mutex& csMixing = pMixer->MixerCritical();
				if (doLock)
					csMixing.lock();
				else
					csMixing.unlock();
			}
		}
	}
	void UpdateSoundStream()
	{
		if (soundStream)
		{
			soundStream->GetMixer()->SetThrottle(SConfig::GetInstance().m_Framelimit == 2);
			soundStream->SetVolume(SConfig::GetInstance().m_Volume);
		}
	}
}
