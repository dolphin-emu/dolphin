// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "AudioCommon/AlsaSoundStream.h"
#include "AudioCommon/AOSoundStream.h"
#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/CoreAudioSoundStream.h"
#include "AudioCommon/DSoundStream.h"
#include "AudioCommon/Mixer.h"
#include "AudioCommon/NullSoundStream.h"
#include "AudioCommon/OpenALStream.h"
#include "AudioCommon/OpenSLESStream.h"
#include "AudioCommon/PulseAudioStream.h"
#include "AudioCommon/XAudio2_7Stream.h"
#include "AudioCommon/XAudio2Stream.h"

#include "Common/FileUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Movie.h"

// This shouldn't be a global, at least not here.
SoundStream *soundStream = nullptr;

namespace AudioCommon
{
	SoundStream *InitSoundStream(void *hWnd)
	{
		CMixer *mixer = new CMixer(48000);

		// TODO: possible memleak with mixer

		std::string backend = SConfig::GetInstance().sBackend;
		if (backend == BACKEND_OPENAL           && OpenALStream::isValid())
			soundStream = new OpenALStream(mixer);
		else if (backend == BACKEND_NULLSOUND   && NullSound::isValid())
			soundStream = new NullSound(mixer);
		else if (backend == BACKEND_DIRECTSOUND && DSound::isValid())
			soundStream = new DSound(mixer, hWnd);
		else if (backend == BACKEND_XAUDIO2)
		{
			if (XAudio2::isValid())
				soundStream = new XAudio2(mixer);
			else if (XAudio2_7::isValid())
				soundStream = new XAudio2_7(mixer);
		}
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

		if (!soundStream && NullSound::isValid())
		{
			WARN_LOG(DSPHLE, "Could not initialize backend %s, using %s instead.",
				backend.c_str(), BACKEND_NULLSOUND);
			soundStream = new NullSound(mixer);
		}

		if (soundStream)
		{
			UpdateSoundStream();
			if (soundStream->Start())
			{
				if (SConfig::GetInstance().m_DumpAudio)
				{
					std::string audio_file_name = File::GetUserPath(D_DUMPAUDIO_IDX) + "audiodump.wav";
					File::CreateFullPath(audio_file_name);
					mixer->StartLogAudio(audio_file_name);
				}

				return soundStream;
			}
			PanicAlertT("Could not initialize backend %s.", backend.c_str());
		}

		PanicAlertT("Sound backend %s is not valid.", backend.c_str());

		delete soundStream;
		soundStream = nullptr;
		return nullptr;
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
			soundStream = nullptr;
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
		if (XAudio2_7::isValid() || XAudio2::isValid())
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

	void ClearAudioBuffer(bool mute)
	{
		if (soundStream)
			soundStream->Clear(mute);
	}

	void SendAIBuffer(short *samples, unsigned int num_samples)
	{
		if (!soundStream)
			return;

		CMixer* pMixer = soundStream->GetMixer();

		if (pMixer && samples)
		{
			pMixer->PushSamples(samples, num_samples);
		}

		soundStream->Update();
	}
}
