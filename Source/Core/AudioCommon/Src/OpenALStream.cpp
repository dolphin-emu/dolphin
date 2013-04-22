// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "OpenALStream.h"

#if defined HAVE_OPENAL && HAVE_OPENAL

#include "aldlist.h"
#include "DPL2Decoder.h"

static soundtouch::SoundTouch soundTouch;

#pragma warning( push )
#pragma warning( disable : 4351 )

OpenALSoundStream::OpenALSoundStream(CMixer *mixer, void *hWnd /*= NULL*/):
	CBaseSoundStream(mixer),
	m_realtimeBuffer(),
	m_sampleBuffer(),
	m_uiBuffers(),
	m_uiSource(0),
	m_numBuffers(0),
	m_join(false)
{
}

#pragma warning( pop )

OpenALSoundStream::~OpenALSoundStream()
{
}

void OpenALSoundStream::OnSetVolume(u32 volume)
{
	if (m_uiSource > 0)
	{
		alSourcef(m_uiSource, AL_GAIN, ConvertVolume(volume));
	}
}

void OpenALSoundStream::OnUpdate()
{
	m_soundSyncEvent.Set();
}

void OpenALSoundStream::OnFlushBuffers(bool mute)
{
	CBaseSoundStream::SetMuted(mute);

	if (mute)
	{
		soundTouch.clear();
		alSourceStop(m_uiSource);
	}
	else
	{
		alSourcePlay(m_uiSource);
	}
}

//
// AyuanX: Spec says OpenAL1.1 is thread safe already
//
bool OpenALSoundStream::OnPreThreadStart()
{
	m_join = false;
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

	// Initialize DPL2 parameters
	dpl2reset();

	soundTouch.clear();
	return bReturn;
}

void OpenALSoundStream::SoundLoop()
{
	Common::SetCurrentThreadName("Audio thread - openal");
	
	CMixer *mixer = CBaseSoundStream::GetMixer();
	u32 ulFrequency = mixer->GetSampleRate();
	m_numBuffers = Core::g_CoreStartupParameter.iLatency + 2; // OpenAL requires a minimum of two buffers

	memset(m_uiBuffers, 0, m_numBuffers * sizeof(ALuint));
	m_uiSource = 0;

	// Generate some AL Buffers for streaming
	alGenBuffers(m_numBuffers, (ALuint *)m_uiBuffers);
	// Generate a Source to playback the Buffers
	alGenSources(1, &m_uiSource);

	// Short Silence
	memset(m_sampleBuffer, 0, OAL_MAX_SAMPLES * SIZE_FLOAT * SURROUND_CHANNELS * m_numBuffers);
	memset(m_realtimeBuffer, 0, OAL_MAX_SAMPLES * 4);
	for (int i = 0; i < m_numBuffers; i++)
	{
#if !defined(__APPLE__)
		if (Core::g_CoreStartupParameter.bDPL2Decoder)
			alBufferData(m_uiBuffers[i], AL_FORMAT_51CHN32, m_sampleBuffer, 4 * SIZE_FLOAT * SURROUND_CHANNELS, ulFrequency);
		else
#endif
			alBufferData(m_uiBuffers[i], AL_FORMAT_STEREO16, m_realtimeBuffer, 4 * 2 * 2, ulFrequency);
	}
	alSourceQueueBuffers(m_uiSource, m_numBuffers, m_uiBuffers);
	alSourcePlay(m_uiSource);
	
	// Set the default sound volume as saved in the config file.
	u32 vol = CBaseSoundStream::GetVolume();
	alSourcef(m_uiSource, AL_GAIN, ConvertVolume(vol)); 

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

	while (!m_join)
	{
		// num_samples_to_render in this update - depends on SystemTimers::AUDIO_DMA_PERIOD.
		const u32 stereo_16_bit_size = 4;
		const u32 dma_length = 32;
		const u64 ais_samples_per_second = 48000 * stereo_16_bit_size;
		u64 audio_dma_period = SystemTimers::GetTicksPerSecond() / (AudioInterface::GetAIDSampleRate() * stereo_16_bit_size / dma_length);
		u64 num_samples_to_render = (audio_dma_period * ais_samples_per_second) / SystemTimers::GetTicksPerSecond();

		unsigned int num_samples = (unsigned int)num_samples_to_render;
		unsigned int minSamples = surround_capable ? 240 : 0; // DPL2 accepts 240 samples minimum (FWRDURATION)

		num_samples = (num_samples > OAL_MAX_SAMPLES) ? OAL_MAX_SAMPLES : num_samples;
		num_samples = mixer->Mix(m_realtimeBuffer, num_samples);

		// Convert the samples from short to float
		float dest[OAL_MAX_SAMPLES * 2 * 2 * OAL_MAX_BUFFERS];
		for (u32 i = 0; i < num_samples; ++i)
		{
			dest[i * 2 + 0] = (float)m_realtimeBuffer[i * 2 + 0] / (1 << 16);
			dest[i * 2 + 1] = (float)m_realtimeBuffer[i * 2 + 1] / (1 << 16);
		}

		soundTouch.putSamples(dest, num_samples);

		if (iBuffersProcessed == iBuffersFilled)
		{
			alGetSourcei(m_uiSource, AL_BUFFERS_PROCESSED, &iBuffersProcessed);
			iBuffersFilled = 0;
		}

		if (iBuffersProcessed)
		{
			float rate = mixer->GetCurrentSpeed();
			if (rate <= 0)
			{
				Core::RequestRefreshInfo();
				rate = mixer->GetCurrentSpeed();
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

			unsigned int nSamples = soundTouch.receiveSamples(m_sampleBuffer, OAL_MAX_SAMPLES * SIZE_FLOAT * OAL_MAX_BUFFERS);

			if (nSamples > minSamples)
			{
				// Remove the Buffer from the Queue.  (uiBuffer contains the Buffer ID for the unqueued Buffer)
				if (iBuffersFilled == 0)
				{
					alSourceUnqueueBuffers(m_uiSource, iBuffersProcessed, uiBufferTemp);
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
					dpl2decode(m_sampleBuffer, nSamples, dpl2);

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
						alBufferData(uiBufferTemp[iBuffersFilled], AL_FORMAT_STEREO_FLOAT32, m_sampleBuffer, nSamples * 4 * 2, ulFrequency);
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
							stereo[i * 2 + 0] = (short)((float)m_sampleBuffer[i * 2 + 0] * (1 << 16));
							stereo[i * 2 + 1] = (short)((float)m_sampleBuffer[i * 2 + 1] * (1 << 16));
						}
						alBufferData(uiBufferTemp[iBuffersFilled], AL_FORMAT_STEREO16, stereo, nSamples * 2 * 2, ulFrequency);
					}
				}

				alSourceQueueBuffers(m_uiSource, 1, &uiBufferTemp[iBuffersFilled]);
				ALenum err = alGetError();
				if (err != 0)
				{
					ERROR_LOG(AUDIO, "Error queuing buffers: %08x", err);
				}
				iBuffersFilled++;

				if (iBuffersFilled == m_numBuffers)
				{
					alSourcePlay(m_uiSource);
					err = alGetError();
					if (err != 0)
					{
						ERROR_LOG(AUDIO, "Error occurred during playback: %08x", err);
					}
				}

				alGetSourcei(m_uiSource, AL_SOURCE_STATE, &iState);
				if (iState != AL_PLAYING)
				{
					// Buffer underrun occurred, resume playback
					alSourcePlay(m_uiSource);
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
			m_soundSyncEvent.Wait();
		}
	}
}

void OpenALSoundStream::OnPreThreadJoin()
{
	m_join = true;
	// kick the thread if it's waiting
	m_soundSyncEvent.Set();
	soundTouch.clear();
}

void OpenALSoundStream::OnPostThreadJoin()
{
	alSourceStop(m_uiSource);
	alSourcei(m_uiSource, AL_BUFFER, 0);

	// Clean up buffers and sources
	alDeleteSources(1, &m_uiSource);
	m_uiSource = 0;
	alDeleteBuffers(m_numBuffers, m_uiBuffers);

	ALCcontext *pContext = alcGetCurrentContext();
	ALCdevice *pDevice = alcGetContextsDevice(pContext);

	alcMakeContextCurrent(NULL);
	alcDestroyContext(pContext);
	alcCloseDevice(pDevice);
}

#endif //HAVE_OPENAL
