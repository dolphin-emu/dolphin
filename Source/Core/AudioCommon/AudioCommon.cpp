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
#include "AudioCommon/WASAPIStream.h"
#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Core/ConfigManager.h"

#ifdef _WIN32
#include <Audioclient.h>
#include <comdef.h>
#include <mmdeviceapi.h>
#endif

// This shouldn't be a global, at least not here.
std::unique_ptr<SoundStream> g_sound_stream;

std::mutex g_sound_stream_mutex;
std::mutex g_sound_stream_running_mutex;

namespace AudioCommon
{
static bool s_audio_dump_start = false;
static bool s_sound_stream_running = false;

constexpr int AUDIO_VOLUME_MIN = 0;
constexpr int AUDIO_VOLUME_MAX = 100;

static std::unique_ptr<SoundStream> CreateSoundStreamForBackend(std::string_view backend)
{
  // We only check IsValid on backends that are only available on some platforms
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

void InitSoundStream()
{
  {
    WARN_LOG_FMT(AUDIO, "Unknown backend {}, using {} instead.", backend, GetDefaultSoundBackend());
    backend = GetDefaultSoundBackend();
    g_sound_stream = CreateSoundStreamForBackend(GetDefaultSoundBackend());
  }

  if (!g_sound_stream || !g_sound_stream->Init())
  {
    WARN_LOG_FMT(AUDIO, "Could not initialize backend {}, using {} instead.", backend,
                 BACKEND_NULLSOUND);
    g_sound_stream = std::make_unique<NullSound>();
    g_sound_stream->Init();
  }
}

void PostInitSoundStream()
{
  // This needs to be called after AudioInterface::Init where input sample rates are set
  UpdateSoundStream();
    std::lock_guard<std::mutex> guard(g_sound_stream_mutex);

    std::string backend = SConfig::GetInstance().sBackend;
    g_sound_stream = CreateSoundStreamForBackend(backend);

    if (!g_sound_stream)
    {
      WARN_LOG(AUDIO, "Unknown backend %s, using %s instead", backend.c_str(),
               GetDefaultSoundBackend().c_str());
      backend = GetDefaultSoundBackend();
      g_sound_stream = CreateSoundStreamForBackend(GetDefaultSoundBackend());
    }

    if (!g_sound_stream || !g_sound_stream->Init())
    {
      WARN_LOG(AUDIO, "Could not initialize backend %s, using %s instead", backend.c_str(),
               BACKEND_NULLSOUND);
      g_sound_stream = std::make_unique<NullSound>();
      g_sound_stream->Init();
    }
  }

  UpdateSoundStreamSettings(true);
  // This can fail, but we don't really care as it just won't produce any sounds,
  // also the user might be able to fix it up by changing his device settings
  // and pausing and unpausing the emulation.
  // Note that when we start a game, this is called here, but then it's called
  // with false and true again, so basically the backend is restarted
  SetSoundStreamRunning(true);

  if (SConfig::GetInstance().m_DumpAudio && !s_audio_dump_start)
    StartAudioDump();
}

void ShutdownSoundStream()
{
  INFO_LOG_FMT(AUDIO, "Shutting down sound stream");

  if (SConfig::GetInstance().m_DumpAudio && s_audio_dump_start)
    StopAudioDump();

  SetSoundStreamRunning(false);
  {
    std::lock_guard<std::mutex> guard(g_sound_stream_mutex);
    g_sound_stream.reset();
  }

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
  return DPL2Quality::Low;
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
  if (backend == BACKEND_OPENAL)
    return true;
  if (backend == BACKEND_CUBEB)
    return true;
  if (backend == BACKEND_PULSEAUDIO)
    return true;
  if (backend == BACKEND_WASAPI)
    return true;
  return false;
}

bool SupportsLatencyControl(std::string_view backend)
{
  // TODO: we should ask the backends whether they support this
#ifdef _WIN32
  // Cubeb only supports custom latency on Windows
  return backend == BACKEND_OPENAL || backend == BACKEND_WASAPI || backend == BACKEND_CUBEB; //To review: not true
#else
  return false;
#endif
}

bool SupportsVolumeChanges(std::string_view backend)
{
  // TODO: we should ask the backends whether they support this
  return backend == BACKEND_CUBEB || backend == BACKEND_OPENAL || backend == BACKEND_WASAPI ||
         BACKEND_OPENSLES;
}

bool SupportsRuntimeSettingsChanges()
{
  std::lock_guard<std::mutex> guard(g_sound_stream_mutex);
  if (g_sound_stream)
  {
    return g_sound_stream->SupportsRuntimeSettingsChanges();
  }
  return false;
}

unsigned long GetDefaultSampleRate()
{
  return 48000ul;
}

unsigned long GetMaxSupportedLatency()
{
  return (Mixer::MAX_SAMPLES * 1000) / (54000000.0 / 1124.0);
}

unsigned long GetUserTargetLatency()
{
  // iLatency is not unsigned
  unsigned long target_latency = std::max(SConfig::GetInstance().iLatency, 0);
  return std::min(target_latency, AudioCommon::GetMaxSupportedLatency());
}

unsigned long GetOSMixerSampleRate()
{
#ifdef _WIN32
  HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (result != RPC_E_CHANGED_MODE && FAILED(result))
  {
    ERROR_LOG(AUDIO, "Failed to initialize the COM library");
    return 0;
  }
  Common::ScopeGuard uninit([result] {
    if (SUCCEEDED(result))
      CoUninitialize();
  });

  IMMDeviceEnumerator* enumerator = nullptr;
  IMMDevice* device = nullptr;
  IAudioClient* audio_client = nullptr;

  result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                            __uuidof(IMMDeviceEnumerator), reinterpret_cast<LPVOID*>(&enumerator));

  if (result != S_OK)
  {
    _com_error err(result);
    std::string error = TStrToUTF8(err.ErrorMessage()).c_str();
    ERROR_LOG(AUDIO, "Failed to create MMDeviceEnumerator: (%s)", error.c_str());
    return 0;
  }

  result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);

  if (result != S_OK)
  {
    _com_error err(result);
    std::string error = TStrToUTF8(err.ErrorMessage()).c_str();
    ERROR_LOG(AUDIO, "Failed to obtain default endpoint: (%s)", error.c_str());
    enumerator->Release();
    return 0;
  }

  result = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
                            reinterpret_cast<LPVOID*>(&audio_client));

  if (result != S_OK)
  {
    _com_error err(result);
    std::string error = TStrToUTF8(err.ErrorMessage()).c_str();
    ERROR_LOG(AUDIO, "Failed to reactivate IAudioClient: (%s)", error.c_str());
    device->Release();
    enumerator->Release();
    return 0;
  }

  WAVEFORMATEX* mixer_format;
  result = audio_client->GetMixFormat(&mixer_format);

  DWORD sample_rate;

  if (result != S_OK)
  {
    _com_error err(result);
    std::string error = TStrToUTF8(err.ErrorMessage()).c_str();
    ERROR_LOG(AUDIO, "Failed to retrieve the mixer format: (%s)", error.c_str());
    // Return the default Dolphin sample rate, hoping it would work
    sample_rate = GetDefaultSampleRate();
  }
  else
  {
    sample_rate = mixer_format->nSamplesPerSec;
    CoTaskMemFree(mixer_format);
  }

  device->Release();
  audio_client->Release();
  enumerator->Release();

  return sample_rate;
#else
  // TODO: implement on other OSes
  return GetDefaultSampleRate();
#endif
}

void UpdateSoundStreamSettings(bool volume_only)
{
  // This can be called from any threads, needs to be protected
  std::lock_guard<std::mutex> guard(g_sound_stream_mutex);
  if (g_sound_stream)
  {
    int volume = SConfig::GetInstance().m_IsMuted ? 0 : SConfig::GetInstance().m_Volume;
    g_sound_stream->SetVolume(volume);

    if (!volume_only)
    {
      // Some backends will be able to apply changes in settings at runtime
      g_sound_stream->OnSettingsChanged();
    }
  }
}

bool SetSoundStreamRunning(bool running, bool send_error)
{
  // This can be called by the main thread while a previous
  // SetRunning() might still be waiting to finish, so we need to protect it
  // as most backends could even crash
  std::lock_guard<std::mutex> guard(g_sound_stream_running_mutex);

  if (!g_sound_stream)
    return true;

  // Safeguard against calling "SetRunning" with a value equal to the previous one,
  // which is not supported on some backends (could crash)
  if (s_sound_stream_running == running)
    return true;

  if (g_sound_stream->SetRunning(running))
  {
    s_sound_stream_running = running;
    return true;
  }
  if (send_error)
  {
    if (running)
    ERROR_LOG_FMT(AUDIO, "Error starting stream.");
    else
    ERROR_LOG_FMT(AUDIO, "Error stopping stream.");
  }
  return false;
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
    pMixer->PushDMASamples(samples, num_samples);
  }

  // Update is called here because DMA are submitted with a lower frequency
  // than DVD/streaming samples. Call it even if s_sound_stream_running
  // is false as some backends try to re-initialize here
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
  if (!g_sound_stream)
    return;
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
  UpdateSoundStreamSettings(true);
}

void DecreaseVolume(unsigned short offset)
{
  SConfig::GetInstance().m_IsMuted = false;
  int& currentVolume = SConfig::GetInstance().m_Volume;
  currentVolume -= offset;
  if (currentVolume < AUDIO_VOLUME_MIN)
    currentVolume = AUDIO_VOLUME_MIN;
  UpdateSoundStreamSettings(true);
}

void ToggleMuteVolume()
{
  bool& isMuted = SConfig::GetInstance().m_IsMuted;
  isMuted = !isMuted;
  UpdateSoundStreamSettings(true);
}
}  // namespace AudioCommon
