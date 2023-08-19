// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/AudioCommon.h"

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "AudioCommon/AlsaSoundStream.h"
#include "AudioCommon/CubebStream.h"
#include "AudioCommon/Mixer.h"
#include "AudioCommon/NullSoundStream.h"
#include "AudioCommon/OpenALStream.h"
#include "AudioCommon/OpenSLESStream.h"
#include "AudioCommon/PulseAudioStream.h"
#include "AudioCommon/WASAPIStream.h"
#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/System.h"

namespace AudioCommon
{
constexpr int AUDIO_VOLUME_MIN = 0;
constexpr int AUDIO_VOLUME_MAX = 100;

static std::unique_ptr<SoundStream> CreateSoundStreamForBackend(std::string_view backend)
{
  if (backend == BACKEND_CUBEB)
    return std::make_unique<CubebStream>();
  else if (backend == BACKEND_OPENAL && OpenALStream::IsValid())
    return std::make_unique<OpenALStream>();
  else if (backend == BACKEND_NULLSOUND)
    return std::make_unique<NullSound>();
  else if (backend == BACKEND_ALSA && AlsaSound::IsValid())
    return std::make_unique<AlsaSound>();
  else if (backend == BACKEND_PULSEAUDIO && PulseAudio::IsValid())
    return std::make_unique<PulseAudio>();
  else if (backend == BACKEND_OPENSLES && OpenSLESStream::IsValid())
    return std::make_unique<OpenSLESStream>();
  else if (backend == BACKEND_WASAPI && WASAPIStream::IsValid())
    return std::make_unique<WASAPIStream>();
  return {};
}

void InitSoundStream(Core::System& system)
{
  std::string backend = Config::Get(Config::MAIN_AUDIO_BACKEND);
  std::unique_ptr<SoundStream> sound_stream = CreateSoundStreamForBackend(backend);

  if (!sound_stream)
  {
    WARN_LOG_FMT(AUDIO, "Unknown backend {}, using {} instead.", backend, GetDefaultSoundBackend());
    backend = GetDefaultSoundBackend();
    sound_stream = CreateSoundStreamForBackend(backend);
  }

  if (!sound_stream || !sound_stream->Init())
  {
    WARN_LOG_FMT(AUDIO, "Could not initialize backend {}, using {} instead.", backend,
                 BACKEND_NULLSOUND);
    sound_stream = std::make_unique<NullSound>();
    sound_stream->Init();
  }

  system.SetSoundStream(std::move(sound_stream));
}

void PostInitSoundStream(Core::System& system)
{
  // This needs to be called after AudioInterface::Init and SerialInterface::Init (for GBA devices)
  // where input sample rates are set
  UpdateSoundStream(system);
  SetSoundStreamRunning(system, true);

  if (Config::Get(Config::MAIN_DUMP_AUDIO) && !system.IsAudioDumpStarted())
    StartAudioDump(system);
}

void ShutdownSoundStream(Core::System& system)
{
  INFO_LOG_FMT(AUDIO, "Shutting down sound stream");

  if (Config::Get(Config::MAIN_DUMP_AUDIO) && system.IsAudioDumpStarted())
    StopAudioDump(system);

  SetSoundStreamRunning(system, false);
  system.SetSoundStream(nullptr);

  INFO_LOG_FMT(AUDIO, "Done shutting down sound stream");
}

std::string GetDefaultSoundBackend()
{
  std::string backend = BACKEND_NULLSOUND;
#if defined ANDROID
  backend = BACKEND_OPENSLES;
#elif defined __linux__
  if (AlsaSound::IsValid())
    backend = BACKEND_ALSA;
#elif defined(__APPLE__) || defined(_WIN32)
  backend = BACKEND_CUBEB;
#endif
  return backend;
}

DPL2Quality GetDefaultDPL2Quality()
{
  return DPL2Quality::High;
}

std::vector<std::string> GetSoundBackends()
{
  std::vector<std::string> backends;

  backends.emplace_back(BACKEND_NULLSOUND);
  backends.emplace_back(BACKEND_CUBEB);
  if (AlsaSound::IsValid())
    backends.emplace_back(BACKEND_ALSA);
  if (PulseAudio::IsValid())
    backends.emplace_back(BACKEND_PULSEAUDIO);
  if (OpenALStream::IsValid())
    backends.emplace_back(BACKEND_OPENAL);
  if (OpenSLESStream::IsValid())
    backends.emplace_back(BACKEND_OPENSLES);
  if (WASAPIStream::IsValid())
    backends.emplace_back(BACKEND_WASAPI);

  return backends;
}

bool SupportsDPL2Decoder(std::string_view backend)
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

bool SupportsLatencyControl(std::string_view backend)
{
  return backend == BACKEND_OPENAL || backend == BACKEND_WASAPI;
}

bool SupportsVolumeChanges(std::string_view backend)
{
  // FIXME: this one should ask the backend whether it supports it.
  //       but getting the backend from string etc. is probably
  //       too much just to enable/disable a stupid slider...
  return backend == BACKEND_CUBEB || backend == BACKEND_OPENAL || backend == BACKEND_WASAPI;
}

void UpdateSoundStream(Core::System& system)
{
  SoundStream* sound_stream = system.GetSoundStream();

  if (sound_stream)
  {
    int volume = Config::Get(Config::MAIN_AUDIO_MUTED) ? 0 : Config::Get(Config::MAIN_AUDIO_VOLUME);
    sound_stream->SetVolume(volume);
  }
}

void SetSoundStreamRunning(Core::System& system, bool running)
{
  SoundStream* sound_stream = system.GetSoundStream();

  if (!sound_stream)
    return;

  if (system.IsSoundStreamRunning() == running)
    return;
  system.SetSoundStreamRunning(running);

  if (sound_stream->SetRunning(running))
    return;
  if (running)
    ERROR_LOG_FMT(AUDIO, "Error starting stream.");
  else
    ERROR_LOG_FMT(AUDIO, "Error stopping stream.");
}

void SendAIBuffer(Core::System& system, const short* samples, unsigned int num_samples)
{
  SoundStream* sound_stream = system.GetSoundStream();

  if (!sound_stream)
    return;

  if (Config::Get(Config::MAIN_DUMP_AUDIO) && !system.IsAudioDumpStarted())
    StartAudioDump(system);
  else if (!Config::Get(Config::MAIN_DUMP_AUDIO) && system.IsAudioDumpStarted())
    StopAudioDump(system);

  Mixer* mixer = sound_stream->GetMixer();

  if (mixer && samples)
  {
    mixer->PushSamples(samples, num_samples);
  }
}

void StartAudioDump(Core::System& system)
{
  SoundStream* sound_stream = system.GetSoundStream();

  std::time_t start_time = std::time(nullptr);

  std::string path_prefix = File::GetUserPath(D_DUMPAUDIO_IDX) + SConfig::GetInstance().GetGameID();

  std::string base_name =
      fmt::format("{}_{:%Y-%m-%d_%H-%M-%S}", path_prefix, fmt::localtime(start_time));

  const std::string audio_file_name_dtk = fmt::format("{}_dtkdump.wav", base_name);
  const std::string audio_file_name_dsp = fmt::format("{}_dspdump.wav", base_name);
  File::CreateFullPath(audio_file_name_dtk);
  File::CreateFullPath(audio_file_name_dsp);
  sound_stream->GetMixer()->StartLogDTKAudio(audio_file_name_dtk);
  sound_stream->GetMixer()->StartLogDSPAudio(audio_file_name_dsp);
  system.SetAudioDumpStarted(true);
}

void StopAudioDump(Core::System& system)
{
  SoundStream* sound_stream = system.GetSoundStream();

  if (!sound_stream)
    return;
  sound_stream->GetMixer()->StopLogDTKAudio();
  sound_stream->GetMixer()->StopLogDSPAudio();
  system.SetAudioDumpStarted(false);
}

void IncreaseVolume(Core::System& system, unsigned short offset)
{
  Config::SetBaseOrCurrent(Config::MAIN_AUDIO_MUTED, false);
  int currentVolume = Config::Get(Config::MAIN_AUDIO_VOLUME);
  currentVolume += offset;
  if (currentVolume > AUDIO_VOLUME_MAX)
    currentVolume = AUDIO_VOLUME_MAX;
  Config::SetBaseOrCurrent(Config::MAIN_AUDIO_VOLUME, currentVolume);
  UpdateSoundStream(system);
}

void DecreaseVolume(Core::System& system, unsigned short offset)
{
  Config::SetBaseOrCurrent(Config::MAIN_AUDIO_MUTED, false);
  int currentVolume = Config::Get(Config::MAIN_AUDIO_VOLUME);
  currentVolume -= offset;
  if (currentVolume < AUDIO_VOLUME_MIN)
    currentVolume = AUDIO_VOLUME_MIN;
  Config::SetBaseOrCurrent(Config::MAIN_AUDIO_VOLUME, currentVolume);
  UpdateSoundStream(system);
}

void ToggleMuteVolume(Core::System& system)
{
  bool isMuted = Config::Get(Config::MAIN_AUDIO_MUTED);
  Config::SetBaseOrCurrent(Config::MAIN_AUDIO_MUTED, !isMuted);
  UpdateSoundStream(system);
}
}  // namespace AudioCommon
