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

static std::array<soundtouch::SoundTouch, SFX_MAX_SOURCE> soundTouch;

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

  for (int i = 0; i < SFX_MAX_SOURCE; ++i)
  {
    soundTouch[i].clear();
  }
  return bReturn;
}

void OpenALStream::Stop()
{
  m_run_thread.Clear();
  // kick the thread if it's waiting
  soundSyncEvent.Set();

  for (int i = 0; i < SFX_MAX_SOURCE; ++i)
  {
    soundTouch[i].clear();
  }

  thread.join();

  for (int i = 0; i < SFX_MAX_SOURCE; ++i)
  {
    alSourceStop(sources[i]);
    alSourcei(sources[i], AL_BUFFER, 0);

    // Clean up buffers and sources
    alDeleteSources(1, &sources[i]);
    sources[i] = 0;
    alDeleteBuffers(num_buffers, buffers[i].data());
  }

  ALCcontext* pContext = alcGetCurrentContext();
  ALCdevice* pDevice = alcGetContextsDevice(pContext);

  alcMakeContextCurrent(nullptr);
  alcDestroyContext(pContext);
  alcCloseDevice(pDevice);
}

void OpenALStream::SetVolume(int set_volume)
{
  volume = static_cast<float>(set_volume) / 100.0f;

  for (int i = 0; i < SFX_MAX_SOURCE; ++i)
  {
    if (sources[i])
      alSourcef(sources[i], AL_GAIN, volume);
  }
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
    for (int i = 0; i < SFX_MAX_SOURCE; ++i)
    {
      soundTouch[i].clear();
      alSourceStop(sources[i]);
    }
  }
  else
  {
    for (int i = 0; i < SFX_MAX_SOURCE; ++i)
    {
      alSourcePlay(sources[i]);
    }
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
  bool use_timestretching = SConfig::GetInstance().bTimeStretching;
  bool float32_capable = false;
  bool fixed32_capable = false;

#if defined(__APPLE__)
  surround_capable = false;
#endif

  if (alIsExtensionPresent("AL_EXT_float32"))
    float32_capable = true;

  // As there is no extension to check for 32-bit fixed point support
  // and we know that only a X-Fi with hardware OpenAL supports it,
  // we just check if one is being used.
  if (strstr(alGetString(AL_RENDERER), "X-Fi"))
    fixed32_capable = true;

  // Clear error state before querying or else we get false positives.
  ALenum err = alGetError();

  // OpenAL requires a minimum of two buffers, three recommended
  num_buffers = 3;

  std::array<u32, SFX_MAX_SOURCE> frequency;
  frequency[0] = m_mixer->GetDMASampleRate();
  frequency[1] = m_mixer->GetStreamingSampleRate();
  frequency[2] = m_mixer->GetWiiMoteSampleRate();

  std::array<u32, SFX_MAX_SOURCE> samples_per_buffer;
  for (int i = 0; i < SFX_MAX_SOURCE; ++i)
  {
    // calculate latency (samples) per buffer
    if (SConfig::GetInstance().iLatency > 10)
    {
      samples_per_buffer[i] = frequency[i] / 1000 * SConfig::GetInstance().iLatency / num_buffers;
    }
    else
    {
      samples_per_buffer[i] = frequency[i] / 1000 * 10 / num_buffers;
    }

    // DPL2 needs a minimum number of samples to work (FWRDURATION) (on any sample rate, or needs
    // only 5ms of data? WiiMote should not use DPL2)
    if (surround_capable && samples_per_buffer[i] < 240 && i != 2)
    {
      samples_per_buffer[i] = 240;
    }

    realtime_buffers[i].resize(samples_per_buffer[i] * STEREO_CHANNELS);
    // SoundTouch can stretch the audio up to 10 times, multiplying by 20 just to be sure.
    sample_buffers[i].resize(samples_per_buffer[i] * STEREO_CHANNELS * 20);
    buffers[i].resize(num_buffers);
    sources[i] = 0;

    // Generate some AL Buffers for streaming
    alGenBuffers(num_buffers, buffers[i].data());
    err = CheckALError("generating buffers");

#ifdef _WIN32
    // Force disable X-RAM, we do not want it for streaming sources
    if (alIsExtensionPresent("EAX-RAM"))
    {
      EAXSetBufferMode eaxSetBufferMode;
      eaxSetBufferMode = (EAXSetBufferMode)alGetProcAddress("EAXSetBufferMode");
      eaxSetBufferMode(num_buffers, buffers[i].data(), alGetEnumValue("AL_STORAGE_ACCESSIBLE"));
      err = CheckALError("setting X-RAM mode");
    }
#endif

    // Generate a Source to playback the Buffers
    alGenSources(1, &sources[i]);
    err = CheckALError("generating sources");

    // Set the default sound volume as saved in the config file.
    alSourcef(sources[i], AL_GAIN, volume);

    // TODO: Error handling
    // ALenum err = alGetError();

    soundTouch[i].setChannels(2);  // Verify if WiiMote is mono
    soundTouch[i].setSampleRate(frequency[i]);
    soundTouch[i].setTempo(1.0);
    soundTouch[i].setSetting(SETTING_USE_QUICKSEEK, 0);
    soundTouch[i].setSetting(SETTING_USE_AA_FILTER, 1);
    soundTouch[i].setSetting(SETTING_SEQUENCE_MS, 1);
    soundTouch[i].setSetting(SETTING_SEEKWINDOW_MS, 28);
    soundTouch[i].setSetting(SETTING_OVERLAP_MS, 12);
  }

  std::array<u32, SFX_MAX_SOURCE> next_buffer = {0, 0, 0};
  std::array<u32, SFX_MAX_SOURCE> num_buffers_queued = {0, 0, 0};
  std::array<ALint, SFX_MAX_SOURCE> state = {0, 0, 0};

  // floating point conversion vector
  std::array<std::vector<float>, SFX_MAX_SOURCE> dest;
  for (int i = 0; i < num_buffers; ++i)
  {
    dest[i].resize(samples_per_buffer[i] * STEREO_CHANNELS);
  }

  while (m_run_thread.IsSet())
  {
    for (int nsource = 0; nsource < SFX_MAX_SOURCE; ++nsource)
    {
      // Used to change the parameters when the frequency changes. Ugly, need to make it better
      bool changed = false;
      switch (nsource)
      {
      case 0:
        if (frequency[nsource] != m_mixer->GetDMASampleRate())
        {
          changed = true;
        }
        break;
      case 1:
        if (frequency[nsource] != m_mixer->GetStreamingSampleRate())
        {
          changed = true;
        }
        break;
      case 2:
        if (frequency[nsource] != m_mixer->GetWiiMoteSampleRate())
        {
          changed = true;
        }
        break;
      }

      if (changed)
      {
        switch (nsource)
        {
        case 0:
          frequency[nsource] = m_mixer->GetDMASampleRate();
          break;
        case 1:
          frequency[nsource] = m_mixer->GetStreamingSampleRate();
          break;
        case 2:
          frequency[nsource] = m_mixer->GetWiiMoteSampleRate();
          break;
        }

        soundTouch[nsource].setSampleRate(frequency[nsource]);

        if (SConfig::GetInstance().iLatency > 10)
        {
          samples_per_buffer[nsource] =
              frequency[nsource] / 1000 * SConfig::GetInstance().iLatency / num_buffers;
        }
        else
        {
          samples_per_buffer[nsource] = frequency[nsource] / 1000 * 10 / num_buffers;
        }

        // Unbind buffers
        alSourceStop(sources[nsource]);
        alSourcei(sources[nsource], AL_BUFFER, 0);

        // Clean up queues
        num_buffers_queued[nsource] = 0;
        next_buffer[nsource] = 0;

        // Resize arrays
        realtime_buffers[nsource].resize(samples_per_buffer[nsource] * STEREO_CHANNELS);
        sample_buffers[nsource].resize(samples_per_buffer[nsource] * STEREO_CHANNELS * 20);

        continue;
      }

      // Block until we have a free buffer
      int num_buffers_processed;
      alGetSourcei(sources[nsource], AL_BUFFERS_PROCESSED, &num_buffers_processed);
      if (num_buffers == num_buffers_queued[nsource] && !num_buffers_processed)
      {
        continue;
      }

      // Remove the Buffer from the Queue.
      if (num_buffers_processed)
      {
        std::unique_ptr<ALuint[]> unqueuedBufferIds(new ALuint[num_buffers]);
        alSourceUnqueueBuffers(sources[nsource], num_buffers_processed, unqueuedBufferIds.get());
        err = CheckALError("unqueuing buffers");

        num_buffers_queued[nsource] -= num_buffers_processed;
      }

      unsigned int rendered_samples = samples_per_buffer[nsource];
      switch (nsource)
      {
      case 0:
        m_mixer->MixDMA(realtime_buffers[nsource].data(), samples_per_buffer[nsource], false, true);
        break;
      case 1:
        m_mixer->MixStreaming(realtime_buffers[nsource].data(), samples_per_buffer[nsource], false,
                              true);
        break;
      case 2:
        m_mixer->MixWiiMote(realtime_buffers[nsource].data(), samples_per_buffer[nsource], false,
                            true);
        break;
      default:
        m_mixer->Mix(realtime_buffers[nsource].data(), samples_per_buffer[nsource], false);
        break;
      }

      if (use_timestretching)
      {
        // Convert the samples from short to float
        for (u32 i = 0; i < rendered_samples * STEREO_CHANNELS; ++i)
          dest[nsource][i] = static_cast<float>(realtime_buffers[nsource][i]) / INT16_MAX;

        soundTouch[nsource].putSamples(dest[nsource].data(), rendered_samples);
      }

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
        if (use_timestretching)
        {
          soundTouch[nsource].setTempo(static_cast<double>(rate));
        }
        else
        {
          alSourcef(sources[nsource], AL_PITCH, rate);
        }

        if (rate > 10 && use_timestretching)
        {
          soundTouch[nsource].clear();
        }
      }

      unsigned int n_samples;
      if (use_timestretching)
      {
        // We want SoundTouch to return already processed samples all at once
        n_samples = soundTouch[nsource].receiveSamples(sample_buffers[nsource].data(),
                                                       soundTouch[nsource].numSamples());
      }
      else
      {
        n_samples = rendered_samples;
      }

      if (n_samples < samples_per_buffer[nsource])
        continue;

      if (surround_capable && nsource != 2)
      {
        if (!use_timestretching)
        {
          // DPL2 decoder accepts only floats
          for (u32 i = 0; i < n_samples * STEREO_CHANNELS; ++i)
            sample_buffers[nsource][i] =
                static_cast<float>(realtime_buffers[nsource][i]) / INT16_MAX;
        }

        std::vector<float> dpl2(n_samples * SURROUND_CHANNELS);
        DPL2Decode(sample_buffers[nsource].data(), n_samples, dpl2.data());

        // zero-out the subwoofer channel - DPL2Decode generates a pretty
        // good 5.0 but not a good 5.1 output.  Sadly there is not a 5.0
        // AL_FORMAT_50CHN32 to make this super-explicit.
        // DPL2Decode output: LEFTFRONT, RIGHTFRONT, CENTREFRONT, (sub), LEFTREAR, RIGHTREAR
        for (u32 i = 0; i < n_samples; ++i)
        {
          dpl2[i * SURROUND_CHANNELS + 3 /*sub/lfe*/] = 0.0f;
        }

        if (float32_capable)
        {
          alBufferData(buffers[nsource][next_buffer[nsource]], AL_FORMAT_51CHN32, dpl2.data(),
                       n_samples * FRAME_SURROUND_FLOAT, frequency[nsource]);
        }
        else if (fixed32_capable)
        {
          std::vector<int> surround_int32(n_samples * SURROUND_CHANNELS);

          for (u32 i = 0; i < n_samples * SURROUND_CHANNELS; ++i)
          {
            // For some reason the ffdshow's DPL2 decoder outputs samples bigger than 1.
            // Most are close to 2.5 and some go up to 8. Hard clamping here, we need to
            // fix the decoder or implement a limiter.
            surround_int32[i] = static_cast<int>(
                MathUtil::Clamp(static_cast<double>(dpl2[i]), -1.0, 1.0) * INT32_MAX);
          }

          alBufferData(buffers[nsource][next_buffer[nsource]], AL_FORMAT_51CHN32,
                       surround_int32.data(), n_samples * FRAME_SURROUND_INT32, frequency[nsource]);
        }
        else
        {
          std::vector<short> surround_short(n_samples * SURROUND_CHANNELS);

          for (u32 i = 0; i < n_samples * SURROUND_CHANNELS; ++i)
          {
            surround_short[i] = static_cast<short>(
                MathUtil::Clamp(static_cast<double>(dpl2[i]), -1.0, 1.0) * INT16_MAX);
          }

          alBufferData(buffers[nsource][next_buffer[nsource]], AL_FORMAT_51CHN16,
                       surround_short.data(), n_samples * FRAME_SURROUND_SHORT, frequency[nsource]);
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
        if (float32_capable && use_timestretching)
        {
          alBufferData(buffers[nsource][next_buffer[nsource]], AL_FORMAT_STEREO_FLOAT32,
                       sample_buffers[nsource].data(), n_samples * FRAME_STEREO_FLOAT,
                       frequency[nsource]);

          err = CheckALError("buffering float32 data");
          if (err == AL_INVALID_ENUM)
          {
            float32_capable = false;
          }
        }
        else if (fixed32_capable && use_timestretching)
        {
          // Clamping is not necessary here, samples are always between (-1,1)
          std::vector<int> stereo_int32(n_samples * STEREO_CHANNELS);
          for (u32 i = 0; i < n_samples * STEREO_CHANNELS; ++i)
            stereo_int32[i] = static_cast<int>(sample_buffers[nsource][i] * INT32_MAX);

          alBufferData(buffers[nsource][next_buffer[nsource]], AL_FORMAT_STEREO32,
                       stereo_int32.data(), n_samples * FRAME_STEREO_INT32, frequency[nsource]);
        }
        else
        {
          // Convert the samples from float to short
          if (use_timestretching)
          {
            std::vector<short> stereo(n_samples * STEREO_CHANNELS);
            for (u32 i = 0; i < n_samples * STEREO_CHANNELS; ++i)
              stereo[i] = static_cast<short>(sample_buffers[nsource][i] * INT16_MAX);

            alBufferData(buffers[nsource][next_buffer[nsource]], AL_FORMAT_STEREO16, stereo.data(),
                         n_samples * FRAME_STEREO_SHORT, frequency[nsource]);
          }
          else
          {
            alBufferData(buffers[nsource][next_buffer[nsource]], AL_FORMAT_STEREO16,
                         realtime_buffers[nsource].data(), n_samples * FRAME_STEREO_SHORT,
                         frequency[nsource]);
          }
        }
      }

      alSourceQueueBuffers(sources[nsource], 1, &buffers[nsource][next_buffer[nsource]]);
      err = CheckALError("queuing buffers");

      num_buffers_queued[nsource]++;
      next_buffer[nsource] = (next_buffer[nsource] + 1) % num_buffers;

      alGetSourcei(sources[nsource], AL_SOURCE_STATE, &state[nsource]);
      if (state[nsource] != AL_PLAYING)
      {
        // Buffer underrun occurred, resume playback
        alSourcePlay(sources[nsource]);
        err = CheckALError("occurred resuming playback");
      }
    }
  }
}

#endif  // HAVE_OPENAL
