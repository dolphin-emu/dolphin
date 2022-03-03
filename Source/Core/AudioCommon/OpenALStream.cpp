// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32

#include "AudioCommon/OpenALStream.h"

#include <windows.h>
#include <climits>
#include <cstring>
#include <thread>

#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"

static HMODULE s_openal_dll = nullptr;

#define OPENAL_API_VISIT(X)                                                                        \
  X(alBufferData)                                                                                  \
  X(alcCloseDevice)                                                                                \
  X(alcCreateContext)                                                                              \
  X(alcDestroyContext)                                                                             \
  X(alcGetContextsDevice)                                                                          \
  X(alcGetCurrentContext)                                                                          \
  X(alcGetString)                                                                                  \
  X(alcIsExtensionPresent)                                                                         \
  X(alcMakeContextCurrent)                                                                         \
  X(alcOpenDevice)                                                                                 \
  X(alDeleteBuffers)                                                                               \
  X(alDeleteSources)                                                                               \
  X(alGenBuffers)                                                                                  \
  X(alGenSources)                                                                                  \
  X(alGetError)                                                                                    \
  X(alGetSourcei)                                                                                  \
  X(alGetString)                                                                                   \
  X(alIsExtensionPresent)                                                                          \
  X(alSourcef)                                                                                     \
  X(alSourcei)                                                                                     \
  X(alSourcePlay)                                                                                  \
  X(alSourceQueueBuffers)                                                                          \
  X(alSourceStop)                                                                                  \
  X(alSourceUnqueueBuffers)

// Create func_t function pointer type and declare a nullptr-initialized static variable of that
// type named "pfunc".
#define DYN_FUNC_DECLARE(func)                                                                     \
  typedef decltype(&func) func##_t;                                                                \
  static func##_t p##func = nullptr;

// Attempt to load the function from the given module handle.
#define OPENAL_FUNC_LOAD(func)                                                                     \
  p##func = (func##_t)::GetProcAddress(s_openal_dll, #func);                                       \
  if (!p##func)                                                                                    \
  {                                                                                                \
    return false;                                                                                  \
  }

OPENAL_API_VISIT(DYN_FUNC_DECLARE);

static bool InitFunctions()
{
  OPENAL_API_VISIT(OPENAL_FUNC_LOAD);
  return true;
}

static bool InitLibrary()
{
  if (s_openal_dll)
    return true;

  s_openal_dll = ::LoadLibrary(TEXT("openal32.dll"));
  if (!s_openal_dll)
    return false;

  if (!InitFunctions())
  {
    ::FreeLibrary(s_openal_dll);
    s_openal_dll = nullptr;
    return false;
  }

  return true;
}

bool OpenALStream::IsValid()
{
  return InitLibrary();
}

//
// AyuanX: Spec says OpenAL1.1 is thread safe already
//
bool OpenALStream::Init()
{
  if (!palcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT"))
  {
    PanicAlertFmtT("OpenAL: can't find sound devices");
    return false;
  }

  const char* default_device_dame = palcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);
  INFO_LOG_FMT(AUDIO, "Found OpenAL device {}", default_device_dame);

  ALCdevice* device = palcOpenDevice(default_device_dame);
  if (!device)
  {
    PanicAlertFmtT("OpenAL: can't open device {0}", default_device_dame);
    return false;
  }

  ALCcontext* context = palcCreateContext(device, nullptr);
  if (!context)
  {
    palcCloseDevice(device);
    PanicAlertFmtT("OpenAL: can't create context for device {0}", default_device_dame);
    return false;
  }

  palcMakeContextCurrent(context);
  m_run_thread.Set();
  m_thread = std::thread(&OpenALStream::SoundLoop, this);
  return true;
}

OpenALStream::~OpenALStream()
{
  m_run_thread.Clear();
  m_thread.join();

  palSourceStop(m_source);
  palSourcei(m_source, AL_BUFFER, 0);

  // Clean up buffers and sources
  palDeleteSources(1, &m_source);
  m_source = 0;
  palDeleteBuffers(OAL_BUFFERS, m_buffers.data());

  ALCcontext* context = palcGetCurrentContext();
  ALCdevice* device = palcGetContextsDevice(context);

  palcMakeContextCurrent(nullptr);
  palcDestroyContext(context);
  palcCloseDevice(device);
}

void OpenALStream::SetVolume(int volume)
{
  m_volume = (float)volume / 100.0f;

  if (m_source)
    palSourcef(m_source, AL_GAIN, m_volume);
}

bool OpenALStream::SetRunning(bool running)
{
  if (running)
  {
    palSourcePlay(m_source);
  }
  else
  {
    palSourceStop(m_source);
  }
  return true;
}

static ALenum CheckALError(const char* desc)
{
  ALenum err = palGetError();

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

    ERROR_LOG_FMT(AUDIO, "Error {}: {:08x} {}", desc, err, type);
  }

  return err;
}

static bool IsCreativeXFi()
{
  return strstr(palGetString(AL_RENDERER), "X-Fi") != nullptr;
}

void OpenALStream::SoundLoop()
{
  Common::SetCurrentThreadName("Audio thread - openal");

  bool float32_capable = palIsExtensionPresent("AL_EXT_float32") != 0;
  bool surround_capable = palIsExtensionPresent("AL_EXT_MCFORMATS") || IsCreativeXFi();
  bool use_surround = Config::ShouldUseDPL2Decoder() && surround_capable;

  // As there is no extension to check for 32-bit fixed point support
  // and we know that only a X-Fi with hardware OpenAL supports it,
  // we just check if one is being used.
  bool fixed32_capable = IsCreativeXFi();

  u32 frequency = m_mixer->GetSampleRate();

  u32 frames_per_buffer;
  // Can't have zero samples per buffer
  if (Config::Get(Config::MAIN_AUDIO_LATENCY) > 0)
  {
    frames_per_buffer = frequency / 1000 * Config::Get(Config::MAIN_AUDIO_LATENCY) / OAL_BUFFERS;
  }
  else
  {
    frames_per_buffer = frequency / 1000 * 1 / OAL_BUFFERS;
  }

  if (frames_per_buffer > OAL_MAX_FRAMES)
  {
    frames_per_buffer = OAL_MAX_FRAMES;
  }

  INFO_LOG_FMT(AUDIO, "Using {} buffers, each with {} audio frames for a total of {}.", OAL_BUFFERS,
               frames_per_buffer, frames_per_buffer * OAL_BUFFERS);

  // Should we make these larger just in case the mixer ever sends more samples
  // than what we request?
  m_realtime_buffer.resize(frames_per_buffer * STEREO_CHANNELS);
  m_source = 0;

  // Clear error state before querying or else we get false positives.
  ALenum err = palGetError();

  // Generate some AL Buffers for streaming
  palGenBuffers(OAL_BUFFERS, (ALuint*)m_buffers.data());
  err = CheckALError("generating buffers");

  // Generate a Source to playback the Buffers
  palGenSources(1, &m_source);
  err = CheckALError("generating sources");

  // Set the default sound volume as saved in the config file.
  palSourcef(m_source, AL_GAIN, m_volume);

  // TODO: Error handling
  // ALenum err = alGetError();

  unsigned int next_buffer = 0;
  unsigned int num_buffers_queued = 0;
  ALint state = 0;

  while (m_run_thread.IsSet())
  {
    // Block until we have a free buffer
    int num_buffers_processed;
    palGetSourcei(m_source, AL_BUFFERS_PROCESSED, &num_buffers_processed);
    if (num_buffers_queued == OAL_BUFFERS && !num_buffers_processed)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    // Remove the Buffer from the Queue.
    if (num_buffers_processed)
    {
      std::array<ALuint, OAL_BUFFERS> unqueued_buffer_ids;
      palSourceUnqueueBuffers(m_source, num_buffers_processed, unqueued_buffer_ids.data());
      err = CheckALError("unqueuing buffers");

      num_buffers_queued -= num_buffers_processed;
    }

    unsigned int min_frames = frames_per_buffer;

    if (use_surround)
    {
      std::array<float, OAL_MAX_FRAMES * SURROUND_CHANNELS> dpl2;
      u32 rendered_frames = m_mixer->MixSurround(dpl2.data(), min_frames);

      if (rendered_frames < min_frames)
        continue;

      if (float32_capable)
      {
        palBufferData(m_buffers[next_buffer], AL_FORMAT_51CHN32, dpl2.data(),
                      rendered_frames * FRAME_SURROUND_FLOAT, frequency);
      }
      else if (fixed32_capable)
      {
        std::array<int, OAL_MAX_FRAMES * SURROUND_CHANNELS> surround_int32;

        for (u32 i = 0; i < rendered_frames * SURROUND_CHANNELS; ++i)
        {
          dpl2[i] = dpl2[i] * std::numeric_limits<int>::max();
          if (dpl2[i] > std::numeric_limits<int>::max())
            surround_int32[i] = std::numeric_limits<int>::max();
          else if (dpl2[i] < std::numeric_limits<int>::min())
            surround_int32[i] = std::numeric_limits<int>::min();
          else
            surround_int32[i] = static_cast<int>(dpl2[i]);
        }

        palBufferData(m_buffers[next_buffer], AL_FORMAT_51CHN32, surround_int32.data(),
                      rendered_frames * FRAME_SURROUND_INT32, frequency);
      }
      else
      {
        std::array<short, OAL_MAX_FRAMES * SURROUND_CHANNELS> surround_short;

        for (u32 i = 0; i < rendered_frames * SURROUND_CHANNELS; ++i)
        {
          dpl2[i] = dpl2[i] * std::numeric_limits<short>::max();
          if (dpl2[i] > std::numeric_limits<short>::max())
            surround_short[i] = std::numeric_limits<short>::max();
          else if (dpl2[i] < std::numeric_limits<short>::min())
            surround_short[i] = std::numeric_limits<short>::min();
          else
            surround_short[i] = static_cast<short>(dpl2[i]);
        }

        palBufferData(m_buffers[next_buffer], AL_FORMAT_51CHN16, surround_short.data(),
                      rendered_frames * FRAME_SURROUND_SHORT, frequency);
      }

      err = CheckALError("buffering data");
      if (err == AL_INVALID_ENUM)
      {
        // 5.1 is not supported by the host, fallback to stereo
        WARN_LOG_FMT(
            AUDIO, "Unable to set 5.1 surround mode.  Updating OpenAL Soft might fix this issue.");
        use_surround = false;
      }
    }
    else
    {
      u32 rendered_frames = m_mixer->Mix(m_realtime_buffer.data(), min_frames);

      if (!rendered_frames)
        continue;

      palBufferData(m_buffers[next_buffer], AL_FORMAT_STEREO16, m_realtime_buffer.data(),
                    rendered_frames * FRAME_STEREO_SHORT, frequency);
    }

    palSourceQueueBuffers(m_source, 1, &m_buffers[next_buffer]);
    err = CheckALError("queuing buffers");

    num_buffers_queued++;
    next_buffer = (next_buffer + 1) % OAL_BUFFERS;

    palGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING)
    {
      // Buffer underrun occurred, resume playback
      palSourcePlay(m_source);
      err = CheckALError("occurred resuming playback");
    }
  }
}

#endif  // _WIN32
