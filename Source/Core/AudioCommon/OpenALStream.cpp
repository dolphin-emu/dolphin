// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "AudioCommon/aldlist.h"
#include "AudioCommon/DPL2Decoder.h"
#include "AudioCommon/OpenALStream.h"

#if defined HAVE_OPENAL && HAVE_OPENAL

//
// AyuanX: Spec says OpenAL1.1 is thread safe already
//
bool OpenALStream::Start()
{
	bool bReturn = false;

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
				thread = std::thread(std::mem_fn(&OpenALStream::SoundLoop), this);
				bReturn = true;
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

	// Initialize DPL2 parameters
	dpl2reset();

	return bReturn;
}

void OpenALStream::Stop()
{
	threadData = 1;
	// kick the thread if it's waiting
	soundSyncEvent.Set();

	thread.join();

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

void OpenALStream::Update()
{
	soundSyncEvent.Set();
}

void OpenALStream::Clear(bool mute)
{
	m_muted = mute;

	if (m_muted)
	{
		alSourceStop(uiSource);
	}
	else
	{
		alSourcePlay(uiSource);
	}
}

void OpenALStream::SoundLoop()
{
	Common::SetCurrentThreadName("Audio thread - openal");

	bool surround_capable = Core::g_CoreStartupParameter.bDPL2Decoder;
#if defined(__APPLE__)
	bool float32_capable = false;
	const ALenum AL_FORMAT_STEREO_FLOAT32 = 0;
	// OSX does not have the alext AL_FORMAT_51CHN32 yet.
	surround_capable = false;
	const ALenum AL_FORMAT_51CHN32 = 0;
#else
	bool float32_capable = true;
#endif

	u32 ulFrequency = m_mixer->GetSampleRate();
	numBuffers = Core::g_CoreStartupParameter.iLatency + 2; // OpenAL requires a minimum of two buffers

	memset(uiBuffers, 0, numBuffers * sizeof(ALuint));
	uiSource = 0;

	// Generate some AL Buffers for streaming
	alGenBuffers(numBuffers, (ALuint *)uiBuffers);
	// Generate a Source to playback the Buffers
	alGenSources(1, &uiSource);

	// Short Silence
	memset(realtimeBuffer, 0, OAL_MAX_SAMPLES * FRAME_STEREO_SHORT);
	for (int i = 0; i < numBuffers; i++)
	{
		alBufferData(uiBuffers[i], AL_FORMAT_STEREO16, realtimeBuffer, 4 * FRAME_STEREO_SHORT, ulFrequency);
	}
	alSourceQueueBuffers(uiSource, numBuffers, uiBuffers);
	alSourcePlay(uiSource);

	// Set the default sound volume as saved in the config file.
	alSourcef(uiSource, AL_GAIN, fVolume);

	while (!threadData)
	{
		int iBuffersProcessed;
		alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &iBuffersProcessed);

		if (iBuffersProcessed)
		{
			ALuint uiBuffer;
			alSourceUnqueueBuffers(uiSource, 1, &uiBuffer);
			ALenum err = alGetError();
			if (err != 0)
			{
				ERROR_LOG(AUDIO, "Error unqueuing buffers: %08x", err);
			}

			const int num_samples = 512;
			m_mixer->Mix(realtimeBuffer, num_samples);
			if (surround_capable) {
				float dpl2in[num_samples * STEREO_CHANNELS];
				float dpl2out[num_samples * SURROUND_CHANNELS];
				for (int i = 0; i < num_samples; ++i)
				{
					dpl2in[i] = realtimeBuffer[i];
				}
				dpl2decode(dpl2in, num_samples, dpl2out);
				alBufferData(uiBuffer, AL_FORMAT_51CHN32, dpl2out, num_samples * FRAME_SURROUND_FLOAT, ulFrequency);
				err = alGetError();
				if (err == AL_INVALID_ENUM)
				{
					// 5.1 is not supported by the host, fallback to stereo
					WARN_LOG(AUDIO, "Unable to set 5.1 surround mode.  Updating OpenAL Soft might fix this issue.");
					surround_capable = false;
					err = 0;
				}
			}
			else
			{
				alBufferData(uiBuffer, AL_FORMAT_STEREO16, realtimeBuffer, num_samples * FRAME_STEREO_SHORT, ulFrequency);
			}
			err = alGetError();
			if (err != 0)
			{
				ERROR_LOG(AUDIO, "Error occurred while buffering data: %08x", err);
			}

			alSourceQueueBuffers(uiSource, 1, &uiBuffer);
			err = alGetError();
			if (err != 0)
			{
				ERROR_LOG(AUDIO, "Error queuing buffers: %08x", err);
			}

			ALint iState = 0;
			alGetSourcei(uiSource, AL_SOURCE_STATE, &iState);
			if (iState != AL_PLAYING)
			{
				// Buffer underrun occurred, resume playback
				alSourcePlay(uiSource);
				err = alGetError();
				if (err != 0)
				{
					ERROR_LOG(AUDIO, "Error occurred resuming playback: %08x", err);
				}
			}
		}
		else
		{
			soundSyncEvent.Wait();
		}
	}
}

#endif //HAVE_OPENAL

