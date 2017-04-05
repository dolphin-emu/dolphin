// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <climits>
#include <cstring>
#include <thread>

#include "AudioCommon/DPL2Decoder.h"
#include "AudioCommon/OpenALStream.h"
#include "AudioCommon/aldlist.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"
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

    INFO_LOG(AUDIO, "Found OpenAL device %s", defDevName);

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
  alDeleteBuffers(numBuffers, uiBuffers.data());

  ALCcontext* pContext = alcGetCurrentContext();
  ALCdevice* pDevice = alcGetContextsDevice(pContext);

  alcMakeContextCurrent(nullptr);
  alcDestroyContext(pContext);
  alcCloseDevice(pDevice);
}

void OpenALStream::SetVolume(int volume)
{
  fVolume = static_cast<float>(volume) / 100.0f;

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

static ALenum CheckALError(const char* desc)
{
  ALenum err = alGetError();

  if (err != AL_NO_ERROR)
  {
    std::string type;

    switch (err)
    {
    case AL_INVALID_NAME:
      type = "AL_INVALID_NAME";
      break;
    case AL_INVALID_ENUM:
      type = "AL_INVALID_ENUM";
      break;
    case AL_INVALID_VALUE:
      type = "AL_INVALID_VALUE";
      break;
    case AL_INVALID_OPERATION:
      type = "AL_INVALID_OPERATION";
      break;
    case AL_OUT_OF_MEMORY:
      type = "AL_OUT_OF_MEMORY";
      break;
    default:
      type = "UNKNOWN_ERROR";
      break;
    }

    ERROR_LOG(AUDIO, "Error %s: %08x %s", desc, err, type.c_str());
  }

  return err;
}

void OpenALStream::SoundLoop()
{
  Common::SetCurrentThreadName("Audio thread - openal");

  bool surround_capable = SConfig::GetInstance().bDPL2Decoder;
  bool float32_capable = false;
  bool fixed32_capable = false;

#if defined(__APPLE__)
  surround_capable = false;
#endif

  u32 ulFrequency = m_mixer->GetSampleRate();
  // OpenAL requires a minimum of two buffers, three recommended
  numBuffers = 3;

  // calculate latency (samples) per buffer
  uint samples_per_buffer;
  if (SConfig::GetInstance().iLatency > 10)
  {
    samples_per_buffer = ulFrequency / 1000 * SConfig::GetInstance().iLatency / numBuffers;
  }
  else
  {
    samples_per_buffer = ulFrequency / 1000 * 10 / numBuffers;
  }

  // DPL2 needs a minimum number of samples to work (FWRDURATION)
  if (surround_capable && samples_per_buffer < 240)
  {
    samples_per_buffer = 240;
  }

  realtimeBuffer.reserve(samples_per_buffer * STEREO_CHANNELS);
  // SoundTouch can stretch the audio up to 10 times, multiplying by 20 just to be sure.
  sampleBuffer.reserve(samples_per_buffer * STEREO_CHANNELS * 20);
  uiBuffers.reserve(numBuffers);
  uiSource = 0;

  if (alIsExtensionPresent("AL_EXT_float32"))
    float32_capable = true;

  // As there is no extension to check for 32-bit fixed point support
  // and we know that only a X-Fi with hardware OpenAL supports it,
  // we just check if one is being used.
  if (strstr(alGetString(AL_RENDERER), "X-Fi"))
    fixed32_capable = true;

  // Clear error state before querying or else we get false positives.
  ALenum err = alGetError();

  // Generate some AL Buffers for streaming
  alGenBuffers(numBuffers, uiBuffers.data());
  err = CheckALError("generating buffers");

#ifdef _WIN32
  // Force disable X-RAM, we do not want it for streaming sources
  if (alIsExtensionPresent("EAX-RAM"))
  {
    EAXSetBufferMode eaxSetBufferMode;
    eaxSetBufferMode = (EAXSetBufferMode)alGetProcAddress("EAXSetBufferMode");
    eaxSetBufferMode(numBuffers, uiBuffers.data(), alGetEnumValue("AL_STORAGE_ACCESSIBLE"));
    err = CheckALError("setting X-RAM mode");
  }
#endif

  // Generate a Source to playback the Buffers
  alGenSources(1, &uiSource);
  err = CheckALError("generating sources");

  // Set the default sound volume as saved in the config file.
  alSourcef(uiSource, AL_GAIN, fVolume);

  // TODO: Error handling
  // ALenum err = alGetError();

  unsigned int nextBuffer = 0;
  unsigned int numBuffersQueued = 0;
  ALint iState = 0;

  soundTouch.setChannels(2);
  soundTouch.setSampleRate(ulFrequency);
  soundTouch.setTempo(1.0);
  soundTouch.setSetting(SETTING_USE_QUICKSEEK, 0);
  soundTouch.setSetting(SETTING_USE_AA_FILTER, 0);
  soundTouch.setSetting(SETTING_SEQUENCE_MS, 1);
  soundTouch.setSetting(SETTING_SEEKWINDOW_MS, 28);
  soundTouch.setSetting(SETTING_OVERLAP_MS, 12);

  // floating point conversion vector
  std::vector<float> dest(samples_per_buffer * STEREO_CHANNELS);

  while (m_run_thread.IsSet())
  {
    // Block until we have a free buffer
    int numBuffersProcessed;
    alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &numBuffersProcessed);
    if (numBuffers == numBuffersQueued && !numBuffersProcessed)
    {
      soundSyncEvent.Wait();
      continue;
    }

    // Remove the Buffer from the Queue.
    if (numBuffersProcessed)
    {
      std::unique_ptr<ALuint[]> unqueuedBufferIds(new ALuint[numBuffers]);
      alSourceUnqueueBuffers(uiSource, numBuffersProcessed, unqueuedBufferIds.get());
      err = CheckALError("unqueuing buffers");

      numBuffersQueued -= numBuffersProcessed;
    }

    // minimum number possible of samples to render in this update - depends on
    // SystemTimers::AUDIO_DMA_PERIOD.
    const u32 stereo_16_bit_size = 4;
    const u32 dma_length = 32;
    const u64 ais_samples_per_second = 48000 * stereo_16_bit_size;
    u64 audio_dma_period = SystemTimers::GetTicksPerSecond() /
                           (AudioInterface::GetAIDSampleRate() * stereo_16_bit_size / dma_length);
    u64 num_samples_to_render =
        (audio_dma_period * ais_samples_per_second) / SystemTimers::GetTicksPerSecond();

    unsigned int minSamples = static_cast<unsigned int>(num_samples_to_render);

    if (samples_per_buffer < minSamples)
    {
      ERROR_LOG(AUDIO, "Current latency too low. Consider increasing it.");
    }

    unsigned int rendered_samples = m_mixer->Mix(realtimeBuffer.data(), samples_per_buffer, false);

    // Convert the samples from short to float
    for (u32 i = 0; i < samples_per_buffer * STEREO_CHANNELS; ++i)
      dest[i] = static_cast<float>(realtimeBuffer[i]) / INT16_MAX;

    soundTouch.putSamples(dest.data(), rendered_samples);

    double rate = static_cast<double>(m_mixer->GetCurrentSpeed());
    if (rate <= 0)
    {
      Core::RequestRefreshInfo();
      rate = static_cast<double>(m_mixer->GetCurrentSpeed());
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

    // We want SoundTouch to return already processed samples all at once
    unsigned int nSamples = soundTouch.receiveSamples(sampleBuffer.data(), soundTouch.numSamples());

    if (nSamples < rendered_samples)
      continue;

    if (surround_capable)
    {
      std::vector<float> dpl2(nSamples * SURROUND_CHANNELS);
      DPL2Decode(sampleBuffer.data(), nSamples, dpl2.data());

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
        alBufferData(uiBuffers[nextBuffer], AL_FORMAT_51CHN32, dpl2.data(),
                     nSamples * FRAME_SURROUND_FLOAT, ulFrequency);
      }
      else if (fixed32_capable)
      {
        std::vector<int> surround_int32(nSamples * SURROUND_CHANNELS);

        for (u32 i = 0; i < nSamples * SURROUND_CHANNELS; ++i)
        {
          // For some reason the ffdshow's DPL2 decoder outputs samples bigger than 1.
          // Most are close to 2.5 and some go up to 8. Hard clamping here, we need to
          // fix the decoder or implement a limiter.
          surround_int32[i] = static_cast<int>(MathUtil::Clamp(static_cast<double>(dpl2[i]), -1.0, 1.0) * INT32_MAX);
        }

        alBufferData(uiBuffers[nextBuffer], AL_FORMAT_51CHN32, surround_int32.data(),
                     nSamples * FRAME_SURROUND_INT32, ulFrequency);
      }
      else
      {
        std::vector<short> surround_short(nSamples * SURROUND_CHANNELS);

        for (u32 i = 0; i < nSamples * SURROUND_CHANNELS; ++i)
        {
          surround_short[i] = static_cast<short>(MathUtil::Clamp(static_cast<double>(dpl2[i]), -1.0, 1.0) * INT16_MAX);
        }

        alBufferData(uiBuffers[nextBuffer], AL_FORMAT_51CHN16, surround_short.data(),
                     nSamples * FRAME_SURROUND_SHORT, ulFrequency);
      }

      err = CheckALError("buffering data");
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
        alBufferData(uiBuffers[nextBuffer], AL_FORMAT_STEREO_FLOAT32, sampleBuffer.data(),
                     nSamples * FRAME_STEREO_FLOAT, ulFrequency);

        err = CheckALError("buffering float32 data");
        if (err == AL_INVALID_ENUM)
        {
          float32_capable = false;
        }
      }
      else if (fixed32_capable)
      {
        // Clamping is not necessary here, samples are always between (-1,1)
        std::vector<int> stereo_int32(nSamples * STEREO_CHANNELS);
        for (u32 i = 0; i < nSamples * STEREO_CHANNELS; ++i)
          stereo_int32[i] = static_cast<int>(sampleBuffer[i] * INT32_MAX);

        alBufferData(uiBuffers[nextBuffer], AL_FORMAT_STEREO32, stereo_int32.data(),
                     nSamples * FRAME_STEREO_INT32, ulFrequency);
      }
      else
      {
        // Convert the samples from float to short
        std::vector<short> stereo(nSamples * STEREO_CHANNELS);
        for (u32 i = 0; i < nSamples * STEREO_CHANNELS; ++i)
          stereo[i] = static_cast<short>(sampleBuffer[i] * INT16_MAX);

        alBufferData(uiBuffers[nextBuffer], AL_FORMAT_STEREO16, stereo.data(),
                     nSamples * FRAME_STEREO_SHORT, ulFrequency);
      }
    }

    alSourceQueueBuffers(uiSource, 1, &uiBuffers[nextBuffer]);
    err = CheckALError("queuing buffers");

    numBuffersQueued++;
    nextBuffer = (nextBuffer + 1) % numBuffers;

    alGetSourcei(uiSource, AL_SOURCE_STATE, &iState);
    if (iState != AL_PLAYING)
    {
      // Buffer underrun occurred, resume playback
      alSourcePlay(uiSource);
      err = CheckALError("occurred resuming playback");
    }
  }
}

#endif  // HAVE_OPENAL
