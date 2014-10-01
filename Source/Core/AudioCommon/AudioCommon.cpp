// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "AudioCommon/AlsaSoundStream.h"
#include "AudioCommon/AOSoundStream.h"
#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/CoreAudioSoundStream.h"
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
SoundStream* g_sound_stream = nullptr;

namespace AudioCommon
{
	SoundStream* InitSoundStream()
	{
		CMixer *mixer = new CMixer(48000);

		// TODO: possible memleak with mixer

		std::string backend = SConfig::GetInstance().sBackend;
		if (backend == BACKEND_OPENAL           && OpenALStream::isValid())
			g_sound_stream = new OpenALStream(mixer);
		else if (backend == BACKEND_NULLSOUND   && NullSound::isValid())
			g_sound_stream = new NullSound(mixer);
		else if (backend == BACKEND_XAUDIO2)
		{
			if (XAudio2::isValid())
				g_sound_stream = new XAudio2(mixer);
			else if (XAudio2_7::isValid())
				g_sound_stream = new XAudio2_7(mixer);
		}
		else if (backend == BACKEND_AOSOUND     && AOSound::isValid())
			g_sound_stream = new AOSound(mixer);
		else if (backend == BACKEND_ALSA        && AlsaSound::isValid())
			g_sound_stream = new AlsaSound(mixer);
		else if (backend == BACKEND_COREAUDIO   && CoreAudioSound::isValid())
			g_sound_stream = new CoreAudioSound(mixer);
		else if (backend == BACKEND_PULSEAUDIO  && PulseAudio::isValid())
			g_sound_stream = new PulseAudio(mixer);
		else if (backend == BACKEND_OPENSLES && OpenSLESStream::isValid())
			g_sound_stream = new OpenSLESStream(mixer);

		if (!g_sound_stream && NullSound::isValid())
		{
			WARN_LOG(DSPHLE, "Could not initialize backend %s, using %s instead.",
				backend.c_str(), BACKEND_NULLSOUND);
			g_sound_stream = new NullSound(mixer);
		}

		if (g_sound_stream)
		{
			UpdateSoundStream();
			if (g_sound_stream->Start())
			{
				if (SConfig::GetInstance().m_DumpAudio)
				{
					std::string audio_file_name = File::GetUserPath(D_DUMPAUDIO_IDX) + "audiodump.wav";
					File::CreateFullPath(audio_file_name);
					mixer->StartLogAudio(audio_file_name);
				}

				return g_sound_stream;
			}
			PanicAlertT("Could not initialize backend %s.", backend.c_str());
		}

		PanicAlertT("Sound backend %s is not valid.", backend.c_str());

		delete g_sound_stream;
		g_sound_stream = nullptr;
		return nullptr;
	}

	void ShutdownSoundStream()
	{
		INFO_LOG(DSPHLE, "Shutting down sound stream");

		if (g_sound_stream)
		{
			g_sound_stream->Stop();
			if (SConfig::GetInstance().m_DumpAudio)
				g_sound_stream->GetMixer()->StopLogAudio();
				//g_sound_stream->StopLogAudio();
			delete g_sound_stream;
			g_sound_stream = nullptr;
		}

		INFO_LOG(DSPHLE, "Done shutting down sound stream");
	}

	std::vector<std::string> GetSoundBackends()
	{
		std::vector<std::string> backends;

		if (NullSound::isValid())
			backends.push_back(BACKEND_NULLSOUND);
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
		if (g_sound_stream)
		{
			// audio typically doesn't maintain its own "paused" state
			// (that's already handled by the CPU and whatever else being paused)
			// so it should be good enough to only lock/unlock here.
			CMixer* pMixer = g_sound_stream->GetMixer();
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
		if (g_sound_stream)
		{
			g_sound_stream->SetVolume(SConfig::GetInstance().m_Volume);
		}
	}

	void ClearAudioBuffer(bool mute)
	{
		if (g_sound_stream)
			g_sound_stream->Clear(mute);
	}

	void SendAIBuffer(short *samples, unsigned int num_samples)
	{
		if (!g_sound_stream)
			return;

		CMixer* pMixer = g_sound_stream->GetMixer();

		if (pMixer && samples)
		{
			pMixer->PushSamples(samples, num_samples);
		}

		g_sound_stream->Update();
	}
}
