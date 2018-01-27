// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/AlsaSoundStream.h"
#include "AudioCommon/CubebStream.h"
#include "AudioCommon/Mixer.h"
#include "AudioCommon/NullSoundStream.h"
#include "AudioCommon/OpenALStream.h"
#include "AudioCommon/OpenSLESStream.h"
#include "AudioCommon/PulseAudioStream.h"
#include "AudioCommon/XAudio2Stream.h"
#include "AudioCommon/XAudio2_7Stream.h"
#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"

// This shouldn't be a global, at least not here.
std::unique_ptr<SoundStream> g_sound_stream;

static bool s_audio_dump_start = false;

namespace AudioCommon
{
static const int AUDIO_VOLUME_MIN = 0;
static const int AUDIO_VOLUME_MAX = 100;

void InitSoundStream()
{
  std::string backend = SConfig::GetInstance().sBackend;
  if (backend == BACKEND_CUBEB)
    g_sound_stream = std::make_unique<CubebStream>();
  else if (backend == BACKEND_OPENAL && OpenALStream::isValid())
    g_sound_stream = std::make_unique<OpenALStream>();
  else if (backend == BACKEND_NULLSOUND)
    g_sound_stream = std::make_unique<NullSound>();
  else if (backend == BACKEND_XAUDIO2)
  {
    if (XAudio2::isValid())
      g_sound_stream = std::make_unique<XAudio2>();
    else if (XAudio2_7::isValid())
      g_sound_stream = std::make_unique<XAudio2_7>();
  }
  else if (backend == BACKEND_ALSA && AlsaSound::isValid())
    g_sound_stream = std::make_unique<AlsaSound>();
  else if (backend == BACKEND_PULSEAUDIO && PulseAudio::isValid())
    g_sound_stream = std::make_unique<PulseAudio>();
  else if (backend == BACKEND_OPENSLES && OpenSLESStream::isValid())
    g_sound_stream = std::make_unique<OpenSLESStream>();

  if (!g_sound_stream)
  {
    WARN_LOG(AUDIO, "Could not initialize backend %s, using %s instead.", backend.c_str(),
             BACKEND_NULLSOUND);
    g_sound_stream = std::make_unique<NullSound>();
  }

  if (!g_sound_stream->Start())
  {
    ERROR_LOG(AUDIO, "Could not start backend %s, using %s instead", backend.c_str(),
              BACKEND_NULLSOUND);

    g_sound_stream = std::make_unique<NullSound>();
    g_sound_stream->Start();
  }

  UpdateSoundStream();

  if (SConfig::GetInstance().m_DumpAudio && !s_audio_dump_start)
    StartAudioDump();
}

void ShutdownSoundStream()
{
  INFO_LOG(AUDIO, "Shutting down sound stream");

  if (g_sound_stream)
  {
    g_sound_stream->Stop();

    if (SConfig::GetInstance().m_DumpAudio && s_audio_dump_start)
      StopAudioDump();

    g_sound_stream.reset();
  }

  INFO_LOG(AUDIO, "Done shutting down sound stream");
}

std::string GetDefaultSoundBackend()
{
  std::string backend = BACKEND_NULLSOUND;
#if defined ANDROID
  backend = BACKEND_OPENSLES;
#elif defined __linux__
  if (AlsaSound::isValid())
    backend = BACKEND_ALSA;
#elif defined __APPLE__
  backend = BACKEND_CUBEB;
#elif defined _WIN32
  backend = BACKEND_XAUDIO2;
#endif
  return backend;
}

std::vector<std::string> GetSoundBackends()
{
  std::vector<std::string> backends;

  backends.push_back(BACKEND_NULLSOUND);
  backends.push_back(BACKEND_CUBEB);
  if (XAudio2_7::isValid() || XAudio2::isValid())
    backends.push_back(BACKEND_XAUDIO2);
  if (AlsaSound::isValid())
    backends.push_back(BACKEND_ALSA);
  if (PulseAudio::isValid())
    backends.push_back(BACKEND_PULSEAUDIO);
  if (OpenALStream::isValid())
    backends.push_back(BACKEND_OPENAL);
  if (OpenSLESStream::isValid())
    backends.push_back(BACKEND_OPENSLES);
  return backends;
}

bool SupportsDPL2Decoder(const std::string& backend)
{
#ifndef __APPLE__
  if (backend == BACKEND_OPENAL)
    return true;
#endif
  if (backend == BACKEND_CUBEB)
    return true;
  if (backend == BACKEND_PULSEAUDIO)
    return true;
  return false;
}

bool SupportsLatencyControl(const std::string& backend)
{
  return backend == BACKEND_OPENAL;
}

bool SupportsVolumeChanges(const std::string& backend)
{
  // FIXME: this one should ask the backend whether it supports it.
  //       but getting the backend from string etc. is probably
  //       too much just to enable/disable a stupid slider...
  return backend == BACKEND_CUBEB || backend == BACKEND_OPENAL || backend == BACKEND_XAUDIO2;
}

void UpdateSoundStream()
{
  if (g_sound_stream)
  {
    int volume = SConfig::GetInstance().m_IsMuted ? 0 : SConfig::GetInstance().m_Volume;
    g_sound_stream->SetVolume(volume);
  }
}

void SetSoundStreamRunning(bool running)
{
  if (g_sound_stream)
    g_sound_stream->SetRunning(running);
}

void SendAIBuffer(const short* samples, unsigned int num_samples)
{
  if (!g_sound_stream)
    return;

  if (SConfig::GetInstance().m_DumpAudio && !s_audio_dump_start)
    StartAudioDump();
  else if (!SConfig::GetInstance().m_DumpAudio && s_audio_dump_start)
    StopAudioDump();

  Mixer* pMixer = g_sound_stream->GetMixer();

  if (pMixer && samples)
  {
    pMixer->PushSamples(samples, num_samples);
  }

  g_sound_stream->Update();
}

void StartAudioDump()
{
  std::string audio_file_name_dtk = File::GetUserPath(D_DUMPAUDIO_IDX) + "dtkdump.wav";
  std::string audio_file_name_dsp = File::GetUserPath(D_DUMPAUDIO_IDX) + "dspdump.wav";
  File::CreateFullPath(audio_file_name_dtk);
  File::CreateFullPath(audio_file_name_dsp);
  g_sound_stream->GetMixer()->StartLogDTKAudio(audio_file_name_dtk);
  g_sound_stream->GetMixer()->StartLogDSPAudio(audio_file_name_dsp);
  s_audio_dump_start = true;
}

void StopAudioDump()
{
  g_sound_stream->GetMixer()->StopLogDTKAudio();
  g_sound_stream->GetMixer()->StopLogDSPAudio();
  s_audio_dump_start = false;
}

void IncreaseVolume(unsigned short offset)
{
  SConfig::GetInstance().m_IsMuted = false;
  int& currentVolume = SConfig::GetInstance().m_Volume;
  currentVolume += offset;
  if (currentVolume > AUDIO_VOLUME_MAX)
    currentVolume = AUDIO_VOLUME_MAX;
  UpdateSoundStream();
}

void DecreaseVolume(unsigned short offset)
{
  SConfig::GetInstance().m_IsMuted = false;
  int& currentVolume = SConfig::GetInstance().m_Volume;
  currentVolume -= offset;
  if (currentVolume < AUDIO_VOLUME_MIN)
    currentVolume = AUDIO_VOLUME_MIN;
  UpdateSoundStream();
}

void ToggleMuteVolume()
{
  bool& isMuted = SConfig::GetInstance().m_IsMuted;
  isMuted = !isMuted;
  UpdateSoundStream();
}
}
