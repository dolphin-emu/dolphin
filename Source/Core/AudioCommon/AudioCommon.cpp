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

bool g_selected_sound_stream_failed = false;
std::mutex g_sound_stream_mutex;
std::mutex g_sound_stream_running_mutex;

namespace AudioCommon
{
static bool s_audio_dump_started = false;
static bool s_sound_stream_running = false;

constexpr int AUDIO_VOLUME_MIN = 0;
constexpr int AUDIO_VOLUME_MAX = 100;

static std::unique_ptr<SoundStream> CreateSoundStreamForBackend(std::string_view backend)
{
  // We only check IsValid on backends that are only available on some platforms
  if (backend == CubebStream::GetName() && CubebStream::IsValid())
    return std::make_unique<CubebStream>();
  else if (backend == OpenALStream::GetName() && OpenALStream::IsValid())
    return std::make_unique<OpenALStream>();
  else if (backend == NullSound::GetName())  // NullSound is always valid
    return std::make_unique<NullSound>();
  else if (backend == AlsaSound::GetName() && AlsaSound::IsValid())
    return std::make_unique<AlsaSound>();
  else if (backend == PulseAudio::GetName() && PulseAudio::IsValid())
    return std::make_unique<PulseAudio>();
  else if (backend == OpenSLESStream::GetName() && OpenSLESStream::IsValid())
    return std::make_unique<OpenSLESStream>();
  else if (backend == WASAPIStream::GetName() && WASAPIStream::IsValid())
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
    g_selected_sound_stream_failed = false;

    if (!g_sound_stream)
    {
      WARN_LOG(AUDIO, "Unknown backend %s, using %s instead (default)", backend.c_str(),
               GetDefaultSoundBackend().c_str());
      backend = GetDefaultSoundBackend();
      g_sound_stream = CreateSoundStreamForBackend(backend);
    }

    if (!g_sound_stream || !g_sound_stream->Init())
    {
      WARN_LOG(AUDIO, "Could not initialize backend %s, using %s instead", backend.c_str(),
               NullSound::GetName().c_str());
      g_sound_stream = std::make_unique<NullSound>();
      g_sound_stream->Init();  // NullSound can't fail
      g_selected_sound_stream_failed = true;
    }
  }

  UpdateSoundStreamSettings(true);
  // This can fail, but we don't really care as it just won't produce any sounds,
  // also the user might be able to fix it up by changing their device settings
  // and pausing and unpausing the emulation.
  // Note that when we start a game, this is called here, but then it's called with false
  // and true again, so basically the backend is "restarted" with every emulation state change
  SetSoundStreamRunning(true, true);

  if (SConfig::GetInstance().m_DumpAudio && !s_audio_dump_started)
    StartAudioDump();
}

void ShutdownSoundStream()
{
  INFO_LOG_FMT(AUDIO, "Shutting down sound stream");

  if (SConfig::GetInstance().m_DumpAudio && s_audio_dump_started)
    StopAudioDump();

  SetSoundStreamRunning(false, true);
  {
    std::lock_guard<std::mutex> guard(g_sound_stream_mutex);
    g_selected_sound_stream_failed = false;
    g_sound_stream.reset();
    s_sound_stream_running = false;  // Force this off in case the backend failed to stop
  }

  INFO_LOG_FMT(AUDIO, "Done shutting down sound stream");
}

std::string GetDefaultSoundBackend()
{
  std::string backend = NullSound::GetName();
#if defined ANDROID
  if (OpenSLESStream::IsValid())
    backend = OpenSLESStream::GetName();
#elif defined __linux__
  if (AlsaSound::IsValid())
    backend = AlsaSound::GetName();
#elif defined(__APPLE__) || defined(_WIN32)
  if (CubebStream::IsValid())
    backend = CubebStream::GetName();
#endif
  return backend;
}

std::string GetBackendName()
{
  std::lock_guard<std::mutex> guard(g_sound_stream_mutex);
  // The only case in which the started stream is different from the config one is when it failed
  // to start and fell back to NullSound
  if (g_sound_stream && g_selected_sound_stream_failed)
  {
    return NullSound::GetName();
  }
  return SConfig::GetInstance().sBackend;
}

DPL2Quality GetDefaultDPL2Quality()
{
  return DPL2Quality::Normal;
}

std::vector<std::string> GetSoundBackends()
{
  std::vector<std::string> backends;

  backends.emplace_back(NullSound::GetName());
  if (CubebStream::IsValid())
    backends.emplace_back(CubebStream::GetName());
  if (AlsaSound::IsValid())
    backends.emplace_back(AlsaSound::GetName());
  if (PulseAudio::IsValid())
    backends.emplace_back(PulseAudio::GetName());
  if (OpenALStream::IsValid())
    backends.emplace_back(OpenALStream::GetName());
  if (OpenSLESStream::IsValid())
    backends.emplace_back(OpenSLESStream::GetName());
  if (WASAPIStream::IsValid())
    backends.emplace_back(WASAPIStream::GetName());

  return backends;
}

bool SupportsSurround(std::string_view backend)
{
  if (backend == CubebStream::GetName())
    return CubebStream::SupportsSurround();
  if (backend == AlsaSound::GetName())
    return AlsaSound::SupportsSurround();
  if (backend == PulseAudio::GetName())
    return PulseAudio::SupportsSurround();
  if (backend == OpenALStream::GetName())
    return OpenALStream::SupportsSurround();
  if (backend == OpenSLESStream::GetName())
    return OpenSLESStream::SupportsSurround();
  if (backend == WASAPIStream::GetName())
    return WASAPIStream::SupportsSurround();

  return false;
}

bool SupportsLatencyControl(std::string_view backend)
{
  if (backend == CubebStream::GetName())
    return CubebStream::SupportsCustomLatency();
  if (backend == AlsaSound::GetName())
    return AlsaSound::SupportsCustomLatency();
  if (backend == PulseAudio::GetName())
    return PulseAudio::SupportsCustomLatency();
  if (backend == OpenALStream::GetName())
    return OpenALStream::SupportsCustomLatency();
  if (backend == OpenSLESStream::GetName())
    return OpenSLESStream::SupportsCustomLatency();
  if (backend == WASAPIStream::GetName())
    return WASAPIStream::SupportsCustomLatency();

  return false;
}

bool SupportsVolumeChanges(std::string_view backend)
{
  if (backend == CubebStream::GetName())
    return CubebStream::SupportsVolumeChanges();
  if (backend == AlsaSound::GetName())
    return AlsaSound::SupportsVolumeChanges();
  if (backend == PulseAudio::GetName())
    return PulseAudio::SupportsVolumeChanges();
  if (backend == OpenALStream::GetName())
    return OpenALStream::SupportsVolumeChanges();
  if (backend == OpenSLESStream::GetName())
    return OpenSLESStream::SupportsVolumeChanges();
  if (backend == WASAPIStream::GetName())
    return WASAPIStream::SupportsVolumeChanges();

  return false;
}

bool BackendSupportsRuntimeSettingsChanges()
{
  std::lock_guard<std::mutex> guard(g_sound_stream_mutex);
  if (g_sound_stream)
  {
    return g_sound_stream->SupportsRuntimeSettingsChanges();
  }
  return false;
}

SurroundState GetSurroundState()
{
  std::lock_guard<std::mutex> guard(g_sound_stream_mutex);
  if (g_sound_stream)
  {
    return g_sound_stream->GetSurroundState();
  }
  return SConfig::GetInstance().ShouldUseDPL2Decoder() ? SurroundState::EnabledNotRunning :
                                                         SurroundState::Disabled;
}

unsigned long GetDefaultSampleRate()
{
  return 48000ul;
}

unsigned long GetMaxSupportedLatency()
{
  // On the right we have the highest sample rate supported by the emulation
  return (Mixer::MAX_SAMPLES * 1000) / (54000000.0 / 1124.0);
}

unsigned long GetUserTargetLatency()
{
  // iAudioBackendLatency is not unsigned
  unsigned long target_latency = std::max(SConfig::GetInstance().iAudioBackendLatency, 0);
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

void UpdateSoundStreamSettings(bool volume_changed, bool settings_changed)
{
  // This can be called from any threads, needs to be protected
  std::lock_guard<std::mutex> guard(g_sound_stream_mutex);
  if (g_sound_stream)
  {
    if (volume_changed)
    {
      int volume = SConfig::GetInstance().m_IsMuted ? 0 : SConfig::GetInstance().m_Volume;
      g_sound_stream->SetVolume(volume);
    }
    if (settings_changed)
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
  // as most backends could even crash. No need to check g_sound_stream_mutex
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

  if (SConfig::GetInstance().m_DumpAudio && !s_audio_dump_started)
    StartAudioDump();
  else if (!SConfig::GetInstance().m_DumpAudio && s_audio_dump_started)
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
  s_audio_dump_started = true;
}

void StopAudioDump()
{
  if (!g_sound_stream)
    return;
  g_sound_stream->GetMixer()->StopLogDTKAudio();
  g_sound_stream->GetMixer()->StopLogDSPAudio();
  s_audio_dump_started = false;
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
