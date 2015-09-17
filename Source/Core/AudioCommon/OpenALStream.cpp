// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/aldlist.h"
#include "AudioCommon/DPL2Decoder.h"
#include "AudioCommon/OpenALStream.h"
#include "Core/ConfigManager.h"

#if defined HAVE_OPENAL && HAVE_OPENAL

#ifdef _WIN32
#pragma comment(lib, "openal32.lib")
#endif

//
// AyuanX: Spec says OpenAL1.1 is thread safe already
//
bool OpenALStream::Start()
{
	ALDeviceList pDeviceList;
	if (pDeviceList.GetNumDevices())
	{
		char *defDevName = pDeviceList.GetDeviceName(pDeviceList.GetDefaultDevice());

		WARN_LOG(AUDIO, "Found OpenAL device %s", defDevName);

		ALCdevice *pDevice = alcOpenDevice(defDevName);
		if (pDevice)
		{
			ALCcontext *pContext = alcCreateContext(pDevice, nullptr);
			if (pContext)
			{
				// Used to determine an appropriate period size (2x period = total buffer size)
				//ALCint refresh;
				//alcGetIntegerv(pDevice, ALC_REFRESH, 1, &refresh);
				//period_size_in_millisec = 1000 / refresh;

				alcMakeContextCurrent(pContext);
				return SoundStream::Start();
			}
			else
			{
				alcCloseDevice(pDevice);
				PanicAlertT("OpenAL: can't create context for device %s", defDevName);
			}
		}
		else
		{
			PanicAlertT("OpenAL: can't open device %s", defDevName);
		}
	}
	else
	{
		PanicAlertT("OpenAL: can't find sound devices");
	}

	return false;
}

void OpenALStream::Stop()
{
	SoundStream::Stop();

	alSourceStop(uiSource);
	alSourcei(uiSource, AL_BUFFER, 0);

	// Clean up buffers and sources
	alDeleteSources(1, &uiSource);
	uiSource = 0;
	alDeleteBuffers(numBuffers, uiBuffers);

	ALCcontext *pContext = alcGetCurrentContext();
	ALCdevice *pDevice = alcGetContextsDevice(pContext);

	alcMakeContextCurrent(nullptr);
	alcDestroyContext(pContext);
	alcCloseDevice(pDevice);
}

void OpenALStream::SetVolume(int volume)
{
	fVolume = (float)volume / 100.0f;

	if (uiSource)
		alSourcef(uiSource, AL_GAIN, fVolume);
}

void OpenALStream::Clear(bool mute)
{
	SoundStream::Clear(mute);
	if (m_muted)
	{
		alSourceStop(uiSource);
	}
	else
	{
		alSourcePlay(uiSource);
	}
}

void OpenALStream::InitializeSoundLoop()
{
	bool surroundsupported = SupportSurroundOutput();
#if defined(__APPLE__) // Apple doesn't even define the AL_FORMAT_51CHN16 enum
	format = AL_FORMAT_STEREO16;
#else
	format = surroundsupported ? AL_FORMAT_51CHN16 : AL_FORMAT_STEREO16;
#endif
	samplesize = surroundsupported ? SOUND_SAMPLES_SURROUND : SOUND_SAMPLES_STEREO;
	ulFrequency = m_mixer->GetSampleRate();
	numBuffers = SConfig::GetInstance().iLatency + SOUND_BUFFER_COUNT; // OpenAL requires a minimum of two buffers

	memset(uiBuffers, 0, numBuffers * sizeof(ALuint));
	uiSource = 0;
	// Generate a Source to playback the Buffers
	alGenSources(1, &uiSource);
	// Generate some AL Buffers for streaming
	alGenBuffers(numBuffers, (ALuint *)uiBuffers);
	// Set the default sound volume as saved in the config file.
	alSourcef(uiSource, AL_GAIN, fVolume);
	auto temp = std::make_unique<s16[]>(SOUND_FRAME_SIZE * samplesize);
	memset(temp.get(), 0, SOUND_FRAME_SIZE * samplesize* sizeof(u16));
	for (u8 i = 0; i < numBuffers; i++)
	{
		alBufferData(uiBuffers[i], format, temp.get(), SOUND_FRAME_SIZE * samplesize* sizeof(u16), ulFrequency);
	}
	alSourceQueueBuffers(uiSource, numBuffers, uiBuffers);
	alSourcePlay(uiSource);
}

u32 OpenALStream::SamplesNeeded()
{
	ALint iBuffersProcessed = 0;
	alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &iBuffersProcessed);
	if (iBuffersProcessed <= 0)
		return 0;
	return (u32)(iBuffersProcessed * SOUND_FRAME_SIZE);
}

void OpenALStream::WriteSamples(s16 *src, u32 numsamples)
{
	ALuint buffer = 0;
	alSourceUnqueueBuffers(uiSource, 1, &buffer);
	alBufferData(buffer, format, src, numsamples * samplesize * sizeof(u16), ulFrequency);
	alSourceQueueBuffers(uiSource, 1, &buffer);
	ALenum err = alGetError();
	if (err != 0)
	{
		ERROR_LOG(AUDIO, "Error occurred while buffering data: %08x", err);
	}
	ALint playstate = 0;
	alGetSourcei(uiSource, AL_SOURCE_STATE, &playstate);
	if (playstate != AL_PLAYING)
		alSourcePlay(uiSource);
}

bool OpenALStream::SupportSurroundOutput()
{
#if defined(__APPLE__) // Apple doesn't do surround sound.
	return false;
#endif
	return SConfig::GetInstance().bDPL2Decoder;
}

#endif //HAVE_OPENAL
