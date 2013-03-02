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

#include "aldlist.h"
#include "OpenALStream.h"
#include "DPL2Decoder.h"

#if defined HAVE_OPENAL && HAVE_OPENAL

soundtouch::SoundTouch soundTouch;

//
// AyuanX: Spec says OpenAL1.1 is thread safe already
//
bool OpenALStream::Start()
{
	ALDeviceList *pDeviceList = NULL;
	ALCcontext *pContext = NULL;
	ALCdevice *pDevice = NULL;
	bool bReturn = false;

	pDeviceList = new ALDeviceList();
	if ((pDeviceList) && (pDeviceList->GetNumDevices()))
	{
		char *defDevName = pDeviceList->GetDeviceName(pDeviceList->GetDefaultDevice());

		WARN_LOG(AUDIO, "Found OpenAL device %s", defDevName);

		pDevice = alcOpenDevice(defDevName);
		if (pDevice)
		{
			pContext = alcCreateContext(pDevice, NULL);
			if (pContext)
			{
				// Used to determine an appropriate period size (2x period = total buffer size)
				//ALCint refresh;
				//alcGetIntegerv(pDevice, ALC_REFRESH, 1, &refresh);
				//period_size_in_millisec = 1000 / refresh;

				alcMakeContextCurrent(pContext);
				thread = std::thread(std::mem_fun(&OpenALStream::SoundLoop), this);
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
		delete pDeviceList;
	}
	else
	{
		PanicAlertT("OpenAL: can't find sound devices");
	}

	// Initialise DPL2 parameters
	dpl2reset();

	soundTouch.clear();
	return bReturn;
}

void OpenALStream::Stop()
{
	threadData = 1;
	// kick the thread if it's waiting
	soundSyncEvent.Set();

	soundTouch.clear();

	thread.join();

	alSourceStop(uiSource);
	alSourcei(uiSource, AL_BUFFER, 0);

	// Clean up buffers and sources
	alDeleteSources(1, &uiSource);
	uiSource = 0;
	alDeleteBuffers(numBuffers, uiBuffers);

	ALCcontext *pContext = alcGetCurrentContext();
	ALCdevice *pDevice = alcGetContextsDevice(pContext);

	alcMakeContextCurrent(NULL);
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

	if(m_muted)
	{
		soundTouch.clear();
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

	u32 ulFrequency = m_mixer->GetSampleRate();
	numBuffers = Core::g_CoreStartupParameter.iLatency + 2; // OpenAL requires a minimum of two buffers

	memset(uiBuffers, 0, numBuffers * sizeof(ALuint));
	uiSource = 0;

	// Generate some AL Buffers for streaming
	alGenBuffers(numBuffers, (ALuint *)uiBuffers);
	// Generate a Source to playback the Buffers
	alGenSources(1, &uiSource);

	// Short Silence
	memset(sampleBuffer, 0, OAL_MAX_SAMPLES * SIZE_FLOAT * SURROUND_CHANNELS * numBuffers);
	memset(realtimeBuffer, 0, OAL_MAX_SAMPLES * 4);
	for (int i = 0; i < numBuffers; i++)
	{
#if !defined(__APPLE__)
		if (Core::g_CoreStartupParameter.bDPL2Decoder)
			alBufferData(uiBuffers[i], AL_FORMAT_51CHN32, sampleBuffer, 4 * SIZE_FLOAT * SURROUND_CHANNELS, ulFrequency);
		else
#endif
			alBufferData(uiBuffers[i], AL_FORMAT_STEREO16, realtimeBuffer, 4 * 2 * 2, ulFrequency);
	}
	alSourceQueueBuffers(uiSource, numBuffers, uiBuffers);
	alSourcePlay(uiSource);
	
	// Set the default sound volume as saved in the config file.
	alSourcef(uiSource, AL_GAIN, fVolume); 

	// TODO: Error handling
	//ALenum err = alGetError();

	ALint iBuffersFilled = 0;
	ALint iBuffersProcessed = 0;
	ALint iState = 0;
	ALuint uiBufferTemp[OAL_MAX_BUFFERS] = {0};

	soundTouch.setChannels(2);
	soundTouch.setSampleRate(ulFrequency);
	soundTouch.setTempo(1.0);
	soundTouch.setSetting(SETTING_USE_QUICKSEEK, 0);
	soundTouch.setSetting(SETTING_USE_AA_FILTER, 0);
	soundTouch.setSetting(SETTING_SEQUENCE_MS, 1);
	soundTouch.setSetting(SETTING_SEEKWINDOW_MS, 28);
	soundTouch.setSetting(SETTING_OVERLAP_MS, 12);

	bool surround_capable = Core::g_CoreStartupParameter.bDPL2Decoder;
#if defined(__APPLE__)
	bool float32_capable = false;
#else
	bool float32_capable = true;
#endif

	while (!threadData) 
	{
		// num_samples_to_render in this update - depends on SystemTimers::AUDIO_DMA_PERIOD.
		const u32 stereo_16_bit_size = 4;
		const u32 dma_length = 32;
		const u64 ais_samples_per_second = 48000 * stereo_16_bit_size;
		u64 audio_dma_period = SystemTimers::GetTicksPerSecond() / (AudioInterface::GetAIDSampleRate() * stereo_16_bit_size / dma_length);
		u64 num_samples_to_render = (audio_dma_period * ais_samples_per_second) / SystemTimers::GetTicksPerSecond();

		unsigned int numSamples = (unsigned int)num_samples_to_render;
		unsigned int minSamples = surround_capable ? 240 : 0; // DPL2 accepts 240 samples minimum (FWRDURATION)

		numSamples = (numSamples > OAL_MAX_SAMPLES) ? OAL_MAX_SAMPLES : numSamples;
		numSamples = m_mixer->Mix(realtimeBuffer, numSamples);

		// Convert the samples from short to float
		float dest[OAL_MAX_SAMPLES * 2 * 2 * OAL_MAX_BUFFERS];
		for (u32 i = 0; i < numSamples; ++i)
		{
			dest[i * 2 + 0] = (float)realtimeBuffer[i * 2 + 0] / (1 << 16);
			dest[i * 2 + 1] = (float)realtimeBuffer[i * 2 + 1] / (1 << 16);
		}

		soundTouch.putSamples(dest, numSamples);

		if (iBuffersProcessed == iBuffersFilled)
		{
			alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &iBuffersProcessed);
			iBuffersFilled = 0;
		}

		if (iBuffersProcessed)
		{
			float rate = m_mixer->GetCurrentSpeed();
			if (rate <= 0)
			{
				Core::RequestRefreshInfo();
				rate = m_mixer->GetCurrentSpeed();
			}

			// Place a lower limit of 10% speed.  When a game boots up, there will be
			// many silence samples.  These do not need to be timestretched.
			if (rate > 0.10)
			{
				// Adjust SETTING_SEQUENCE_MS to balance between lag vs hollow audio
				soundTouch.setSetting(SETTING_SEQUENCE_MS, (int)(1 / (rate * rate)));
				soundTouch.setTempo(rate);
				if (rate > 10)
				{
					soundTouch.clear();
				}
			}

			unsigned int nSamples = soundTouch.receiveSamples(sampleBuffer, OAL_MAX_SAMPLES * SIZE_FLOAT * OAL_MAX_BUFFERS);

			if (nSamples > minSamples)
			{
				// Remove the Buffer from the Queue.  (uiBuffer contains the Buffer ID for the unqueued Buffer)
				if (iBuffersFilled == 0)
				{
					alSourceUnqueueBuffers(uiSource, iBuffersProcessed, uiBufferTemp);
					ALenum err = alGetError();
					if (err != 0)
					{
						ERROR_LOG(AUDIO, "Error unqueuing buffers: %08x", err);
					}
				}
#if defined(__APPLE__)
				// OSX does not have the alext AL_FORMAT_51CHN32 yet.
				surround_capable = false;
#else
				if (surround_capable)
				{
					float dpl2[OAL_MAX_SAMPLES * SIZE_FLOAT * SURROUND_CHANNELS * OAL_MAX_BUFFERS];
					dpl2decode(sampleBuffer, nSamples, dpl2);

					alBufferData(uiBufferTemp[iBuffersFilled], AL_FORMAT_51CHN32, dpl2, nSamples * SIZE_FLOAT * SURROUND_CHANNELS, ulFrequency);
					ALenum err = alGetError();
					if (err == AL_INVALID_ENUM)
					{
						// 5.1 is not supported by the host, fallback to stereo
						WARN_LOG(AUDIO, "Unable to set 5.1 surround mode.  Updating OpenAL Soft might fix this issue.");
						surround_capable = false;
					}
					else if (err != 0)
					{
						ERROR_LOG(AUDIO, "Error occurred while buffering data: %08x", err);
					}
				}
#endif
				if (!surround_capable)
				{
#if !defined(__APPLE__)
					if (float32_capable)
					{
						alBufferData(uiBufferTemp[iBuffersFilled], AL_FORMAT_STEREO_FLOAT32, sampleBuffer, nSamples * 4 * 2, ulFrequency);
						ALenum err = alGetError();
						if (err == AL_INVALID_ENUM)
						{
							float32_capable = false;
						}
						else if (err != 0)
						{
							ERROR_LOG(AUDIO, "Error occurred while buffering float32 data: %08x", err);
						}

					}
#endif
					if (!float32_capable)
					{
						// Convert the samples from float to short
						short stereo[OAL_MAX_SAMPLES * 2 * 2 * OAL_MAX_BUFFERS];
						for (u32 i = 0; i < nSamples; ++i)
						{
							stereo[i * 2 + 0] = (short)((float)sampleBuffer[i * 2 + 0] * (1 << 16));
							stereo[i * 2 + 1] = (short)((float)sampleBuffer[i * 2 + 1] * (1 << 16));
						}
						alBufferData(uiBufferTemp[iBuffersFilled], AL_FORMAT_STEREO16, stereo, nSamples * 2 * 2, ulFrequency);

					}
				}

				alSourceQueueBuffers(uiSource, 1, &uiBufferTemp[iBuffersFilled]);
				ALenum err = alGetError();
				if (err != 0)
				{
					ERROR_LOG(AUDIO, "Error queuing buffers: %08x", err);
				}
				iBuffersFilled++;

				if (iBuffersFilled == numBuffers)
				{
					alSourcePlay(uiSource);
					err = alGetError();
					if (err != 0)
					{
						ERROR_LOG(AUDIO, "Error occurred during playback: %08x", err);
					}
				}

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
		}
		else
		{
			soundSyncEvent.Wait();
		}
	}
}

#endif //HAVE_OPENAL

