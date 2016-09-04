// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <thread>

#include "AudioCommon/DPL2Decoder.h"
#include "AudioCommon/OpenALStream.h"
#include "AudioCommon/aldlist.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"

#if defined HAVE_OPENAL && HAVE_OPENAL

#ifdef _WIN32
#pragma comment(lib, "openal32.lib")
#endif

static soundtouch::SoundTouch soundTouch;

//
// AyuanX: Spec says OpenAL1.1 is thread safe already
//
bool OpenALStream::Start()
{
  m_run_thread.Set();
  bool bReturn = false;

  ALDeviceList pDeviceList;
  if (pDeviceList.GetNumDevices())
  {
    char* defDevName = pDeviceList.GetDeviceName(pDeviceList.GetDefaultDevice());

    WARN_LOG(AUDIO, "Found OpenAL device %s", defDevName);

    ALCdevice* pDevice = alcOpenDevice(defDevName);
    if (pDevice)
    {
      ALCcontext* pContext = alcCreateContext(pDevice, nullptr);
      if (pContext)
      {
        // Used to determine an appropriate period size (2x period = total buffer size)
        // ALCint refresh;
        // alcGetIntegerv(pDevice, ALC_REFRESH, 1, &refresh);
        // period_size_in_millisec = 1000 / refresh;

        alcMakeContextCurrent(pContext);
        thread = std::thread(&OpenALStream::SoundLoop, this);
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
  DPL2Reset();

  soundTouch.clear();
  return bReturn;
}

void OpenALStream::Stop()
{
  m_run_thread.Clear();
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

  ALCcontext* pContext = alcGetCurrentContext();
  ALCdevice* pDevice = alcGetContextsDevice(pContext);

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
    soundTouch.clear();
    alSourceStop(uiSource);
  }
  else
  {
    alSourcePlay(uiSource);
  }
}

void OpenALStream::ALCheckError(ALenum* err, const char desc[])
{
  *err = alGetError();

  if (*err != AL_NO_ERROR)
  {
    std::string type;
    if (*err == AL_INVALID_NAME)
    {
      type = "AL_INVALID_NAME";
    }
    else if (*err == AL_INVALID_ENUM)
    {
      type = "AL_INVALID_ENUM";
    }
    else if (*err == AL_INVALID_VALUE)
    {
      type = "AL_INVALID_VALUE";
    }
    else if (*err == AL_INVALID_OPERATION)
    {
      type = "AL_INVALID_OPERATION";
    }
    else if (*err == AL_OUT_OF_MEMORY)
    {
      type = "AL_OUT_OF_MEMORY";
    }
    else
    {
	  type = "UNKNOWN_ERROR";
    }

    ERROR_LOG(AUDIO, "Error %s: %08x %s", desc, *err, type);
  }
}

void OpenALStream::SoundLoop()
{
  Common::SetCurrentThreadName("Audio thread - openal");

  bool surround_capable = SConfig::GetInstance().bDPL2Decoder;
#if defined(__APPLE__)
  bool float32_capable = false;
  const ALenum AL_FORMAT_STEREO_FLOAT32 = 0;
  // OS X does not have the alext AL_FORMAT_51CHN32 yet.
  surround_capable = false;
  const ALenum AL_FORMAT_51CHN32 = 0;
  const ALenum AL_FORMAT_51CHN16 = 0;
#else
  bool float32_capable = true;
#endif

  u32 ulFrequency = m_mixer->GetSampleRate();
  numBuffers = SConfig::GetInstance().iLatency + 2;  // OpenAL requires a minimum of two buffers

  memset(uiBuffers, 0, numBuffers * sizeof(ALuint));
  uiSource = 0;

  // Checks if a X-Fi is being used. If it is, disable FLOAT32 support as this sound card has no
  // support for it even though it reports it does.
  if (strstr(alGetString(AL_RENDERER), "X-Fi"))
    float32_capable = false;

  // Clear error state before querying or else we get false positives.
  ALenum err = alGetError();

  // Generate some AL Buffers for streaming
  alGenBuffers(numBuffers, (ALuint*)uiBuffers);
  ALCheckError(&err, "generating buffers");

  // Generate a Source to playback the Buffers
  alGenSources(1, &uiSource);
  ALCheckError(&err, "generating sources");

  // Short Silence
  if (float32_capable)
    memset(sampleBuffer, 0, OAL_MAX_SAMPLES * numBuffers * FRAME_SURROUND_FLOAT);
  else
    memset(sampleBuffer, 0, OAL_MAX_SAMPLES * numBuffers * FRAME_SURROUND_SHORT);

  memset(realtimeBuffer, 0, OAL_MAX_SAMPLES * FRAME_STEREO_SHORT);

  for (int i = 0; i < numBuffers; i++)
  {
    if (surround_capable)
    {
      if (float32_capable)
        alBufferData(uiBuffers[i], AL_FORMAT_51CHN32, sampleBuffer, 4 * FRAME_SURROUND_FLOAT,
                     ulFrequency);
      else
        alBufferData(uiBuffers[i], AL_FORMAT_51CHN16, sampleBuffer, 4 * FRAME_SURROUND_SHORT,
                     ulFrequency);
    }
    else
    {
      alBufferData(uiBuffers[i], AL_FORMAT_STEREO16, realtimeBuffer, 4 * FRAME_STEREO_SHORT,
                   ulFrequency);
    }
  }
  ALCheckError(&err, "buffering data");

  alSourceQueueBuffers(uiSource, numBuffers, uiBuffers);
  ALCheckError(&err, "queuing buffers");

  alSourcePlay(uiSource);
  ALCheckError(&err, "playing source");

  // Set the default sound volume as saved in the config file.
  alSourcef(uiSource, AL_GAIN, fVolume);

  // TODO: Error handling
  // ALenum err = alGetError();

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

  while (m_run_thread.IsSet())
  {
    // num_samples_to_render in this update - depends on SystemTimers::AUDIO_DMA_PERIOD.
    const u32 stereo_16_bit_size = 4;
    const u32 dma_length = 32;
    const u64 ais_samples_per_second = 48000 * stereo_16_bit_size;
    u64 audio_dma_period = SystemTimers::GetTicksPerSecond() /
                           (AudioInterface::GetAIDSampleRate() * stereo_16_bit_size / dma_length);
    u64 num_samples_to_render =
        (audio_dma_period * ais_samples_per_second) / SystemTimers::GetTicksPerSecond();

    unsigned int numSamples = (unsigned int)num_samples_to_render;
    unsigned int minSamples =
        surround_capable ? 240 : 0;  // DPL2 accepts 240 samples minimum (FWRDURATION)

    numSamples = (numSamples > OAL_MAX_SAMPLES) ? OAL_MAX_SAMPLES : numSamples;
    numSamples = m_mixer->Mix(realtimeBuffer, numSamples, false);

    // Convert the samples from short to float
    float dest[OAL_MAX_SAMPLES * STEREO_CHANNELS];
    for (u32 i = 0; i < numSamples * STEREO_CHANNELS; ++i)
      dest[i] = (float)realtimeBuffer[i] / (1 << 15);

    soundTouch.putSamples(dest, numSamples);

    if (iBuffersProcessed == iBuffersFilled)
    {
      alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &iBuffersProcessed);
      iBuffersFilled = 0;
    }

    if (iBuffersProcessed)
    {
      double rate = (double)m_mixer->GetCurrentSpeed();
      if (rate <= 0)
      {
        Core::RequestRefreshInfo();
        rate = (double)m_mixer->GetCurrentSpeed();
      }

      // Place a lower limit of 10% speed.  When a game boots up, there will be
      // many silence samples.  These do not need to be timestretched.
      if (rate > 0.10)
      {
        soundTouch.setTempo(rate);
        if (rate > 10)
        {
          soundTouch.clear();
        }
      }

      unsigned int nSamples = soundTouch.receiveSamples(sampleBuffer, OAL_MAX_SAMPLES * numBuffers);

      if (nSamples <= minSamples)
        continue;

      // Remove the Buffer from the Queue.  (uiBuffer contains the Buffer ID for the unqueued
      // Buffer)
      if (iBuffersFilled == 0)
      {
        alSourceUnqueueBuffers(uiSource, iBuffersProcessed, uiBufferTemp);
        ALCheckError(&err, "unqueuing buffers");
      }

      if (surround_capable)
      {
        float dpl2[OAL_MAX_SAMPLES * OAL_MAX_BUFFERS * SURROUND_CHANNELS];
        DPL2Decode(sampleBuffer, nSamples, dpl2);

        // zero-out the subwoofer channel - DPL2Decode generates a pretty
        // good 5.0 but not a good 5.1 output.  Sadly there is not a 5.0
        // AL_FORMAT_50CHN32 to make this super-explicit.
        // DPL2Decode output: LEFTFRONT, RIGHTFRONT, CENTREFRONT, (sub), LEFTREAR, RIGHTREAR
        for (u32 i = 0; i < nSamples; ++i)
        {
          dpl2[i * SURROUND_CHANNELS + 3 /*sub/lfe*/] = 0.0f;
        }

        if (float32_capable)
        {
          alBufferData(uiBufferTemp[iBuffersFilled], AL_FORMAT_51CHN32, dpl2,
                       nSamples * FRAME_SURROUND_FLOAT, ulFrequency);
        }
        else
        {
          short surround_short[OAL_MAX_SAMPLES * SURROUND_CHANNELS * OAL_MAX_BUFFERS];
          for (u32 i = 0; i < nSamples * SURROUND_CHANNELS; ++i)
            surround_short[i] = (short)((float)dpl2[i] * (1 << 15));

          alBufferData(uiBufferTemp[iBuffersFilled], AL_FORMAT_51CHN16, surround_short,
                       nSamples * FRAME_SURROUND_SHORT, ulFrequency);
        }

        ALCheckError(&err, "buffering data");
        if (err == AL_INVALID_ENUM)
        {
          // 5.1 is not supported by the host, fallback to stereo
          WARN_LOG(AUDIO,
                   "Unable to set 5.1 surround mode.  Updating OpenAL Soft might fix this issue.");
          surround_capable = false;
        }
      }

      else
      {
        if (float32_capable)
        {
          alBufferData(uiBufferTemp[iBuffersFilled], AL_FORMAT_STEREO_FLOAT32, sampleBuffer,
                       nSamples * FRAME_STEREO_FLOAT, ulFrequency);
          ALCheckError(&err, "buffering float32 data");
          if (err == AL_INVALID_ENUM)
          {
            float32_capable = false;
          }
        }

        else
        {
          // Convert the samples from float to short
          short stereo[OAL_MAX_SAMPLES * STEREO_CHANNELS * OAL_MAX_BUFFERS];
          for (u32 i = 0; i < nSamples * STEREO_CHANNELS; ++i)
            stereo[i] = (short)((float)sampleBuffer[i] * (1 << 15));

          alBufferData(uiBufferTemp[iBuffersFilled], AL_FORMAT_STEREO16, stereo,
                       nSamples * FRAME_STEREO_SHORT, ulFrequency);
        }
      }

      alSourceQueueBuffers(uiSource, 1, &uiBufferTemp[iBuffersFilled]);
      ALCheckError(&err, "queuing buffers");
      iBuffersFilled++;

      if (iBuffersFilled == numBuffers)
      {
        alSourcePlay(uiSource);
        ALCheckError(&err, "occurred during playback");
      }

      alGetSourcei(uiSource, AL_SOURCE_STATE, &iState);
      if (iState != AL_PLAYING)
      {
        // Buffer underrun occurred, resume playback
        alSourcePlay(uiSource);
        ALCheckError(&err, "occurred resuming playback");
      }
    }
    else
    {
      soundSyncEvent.Wait();
    }
  }
}

#endif  // HAVE_OPENAL
