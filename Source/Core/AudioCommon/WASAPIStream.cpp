// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/WASAPIStream.h"

#ifdef _WIN32

// clang-format off
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <devpkey.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wil/resource.h>
// clang-format on

#include <thread>

#include "Common/Assert.h"
#include "Common/HRWrap.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"
#include "VideoCommon/OnScreenDisplay.h"

using Microsoft::WRL::ComPtr;

WASAPIStream::WASAPIStream()
{
  if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
    m_coinitialize.activate();

  m_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  m_format.Format.nChannels = 2;
  m_format.Format.nSamplesPerSec = GetMixer()->GetSampleRate();
  m_format.Format.nAvgBytesPerSec = m_format.Format.nSamplesPerSec * 4;
  m_format.Format.nBlockAlign = 4;
  m_format.Format.wBitsPerSample = 16;
  m_format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  m_format.Samples.wValidBitsPerSample = m_format.Format.wBitsPerSample;
  m_format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
  m_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
}

WASAPIStream::~WASAPIStream()
{
  m_running.store(false, std::memory_order_relaxed);
  if (m_thread.joinable())
    m_thread.join();
}

bool WASAPIStream::IsValid()
{
  return true;
}

static bool HandleWinAPI(std::string_view message, HRESULT result)
{
  if (FAILED(result))
  {
    std::string error;

    switch (result)
    {
    case AUDCLNT_E_DEVICE_IN_USE:
      error = "Audio endpoint already in use!";
      break;
    default:
      error = Common::GetHResultMessage(result);
      break;
    }

    ERROR_LOG_FMT(AUDIO, "WASAPI: {}: {} ({:08x})", message, error, result);
  }

  return SUCCEEDED(result);
}

static void ForEachNamedDevice(const std::function<bool(ComPtr<IMMDevice>, std::string)>& callback)
{
  HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  // RPC_E_CHANGED_MODE means that thread has COM already initialized with a different threading
  // model. We don't necessarily need multithreaded model here, so don't treat this as an error
  if (result != RPC_E_CHANGED_MODE && !HandleWinAPI("Failed to call CoInitialize", result))
    return;

  wil::unique_couninitialize_call cleanup;
  if (FAILED(result))
    cleanup.release();  // CoUninitialize must be matched with each successful CoInitialize call, so
                        // don't call it if initialize fails

  ComPtr<IMMDeviceEnumerator> enumerator;

  result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                            IID_PPV_ARGS(enumerator.GetAddressOf()));

  if (!HandleWinAPI("Failed to create MMDeviceEnumerator", result))
    return;

  ComPtr<IMMDeviceCollection> devices;
  result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, devices.GetAddressOf());

  if (!HandleWinAPI("Failed to get available devices", result))
    return;

  UINT count;
  devices->GetCount(&count);

  for (u32 i = 0; i < count; i++)
  {
    ComPtr<IMMDevice> device;
    devices->Item(i, device.GetAddressOf());
    if (!HandleWinAPI("Failed to get device " + std::to_string(i), result))
      continue;

    ComPtr<IPropertyStore> device_properties;

    result = device->OpenPropertyStore(STGM_READ, device_properties.GetAddressOf());

    if (!HandleWinAPI("Failed to initialize IPropertyStore", result))
      continue;

    wil::unique_prop_variant device_name;
    device_properties->GetValue(PKEY_Device_FriendlyName, device_name.addressof());

    if (!callback(std::move(device), TStrToUTF8(device_name.pwszVal)))
      break;
  }
}

std::vector<std::string> WASAPIStream::GetAvailableDevices()
{
  std::vector<std::string> device_names;

  ForEachNamedDevice([&device_names](ComPtr<IMMDevice>, std::string n) {
    device_names.push_back(std::move(n));
    return true;
  });

  return device_names;
}

ComPtr<IMMDevice> WASAPIStream::GetDeviceByName(std::string_view name)
{
  ComPtr<IMMDevice> device;

  ForEachNamedDevice([&device, &name](ComPtr<IMMDevice> d, std::string n) {
    if (n == name)
    {
      device = std::move(d);
      return false;
    }
    return true;
  });

  return device;
}

bool WASAPIStream::Init()
{
  ASSERT(m_enumerator == nullptr);
  HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(m_enumerator.GetAddressOf()));

  if (!HandleWinAPI("Failed to create MMDeviceEnumerator", result))
    return false;

  return true;
}

bool WASAPIStream::SetRunning(bool running)
{
  if (running)
  {
    ComPtr<IMMDevice> device;

    HRESULT result;

    if (Config::Get(Config::MAIN_WASAPI_DEVICE) == "default")
    {
      result = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.GetAddressOf());
    }
    else
    {
      result = S_OK;
      device = GetDeviceByName(Config::Get(Config::MAIN_WASAPI_DEVICE));

      if (!device)
      {
        ERROR_LOG_FMT(AUDIO, "Can't find device '{}', falling back to default",
                      Config::Get(Config::MAIN_WASAPI_DEVICE));
        result = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.GetAddressOf());
      }
    }

    if (!HandleWinAPI("Failed to obtain default endpoint", result))
      return false;

    // Show a friendly name in the log
    ComPtr<IPropertyStore> device_properties;

    result = device->OpenPropertyStore(STGM_READ, device_properties.GetAddressOf());

    if (!HandleWinAPI("Failed to initialize IPropertyStore", result))
      return false;

    wil::unique_prop_variant device_name;
    device_properties->GetValue(PKEY_Device_FriendlyName, device_name.addressof());

    INFO_LOG_FMT(AUDIO, "Using audio endpoint '{}'", TStrToUTF8(device_name.pwszVal));

    ComPtr<IAudioClient> audio_client;

    // Get IAudioDevice
    result = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
                              reinterpret_cast<LPVOID*>(audio_client.GetAddressOf()));

    if (!HandleWinAPI("Failed to activate IAudioClient", result))
      return false;

    REFERENCE_TIME device_period = 0;

    result = audio_client->GetDevicePeriod(nullptr, &device_period);

    device_period += Config::Get(Config::MAIN_AUDIO_LATENCY) * (10000 / m_format.Format.nChannels);
    INFO_LOG_FMT(AUDIO, "Audio period set to {}", device_period);

    if (!HandleWinAPI("Failed to obtain device period", result))
      return false;

    result = audio_client->Initialize(
        AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST, device_period,
        device_period, reinterpret_cast<WAVEFORMATEX*>(&m_format), nullptr);

    if (result == AUDCLNT_E_UNSUPPORTED_FORMAT)
    {
      OSD::AddMessage("Your current audio device doesn't support 16-bit 48000 hz PCM audio. WASAPI "
                      "exclusive mode won't work.",
                      6000U);
      return false;
    }

    if (result == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
      result = audio_client->GetBufferSize(&m_frames_in_buffer);

      if (!HandleWinAPI("Failed to get aligned buffer size", result))
        return false;

      // Get IAudioDevice
      result = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
                                reinterpret_cast<LPVOID*>(audio_client.ReleaseAndGetAddressOf()));

      if (!HandleWinAPI("Failed to reactivate IAudioClient", result))
        return false;

      device_period =
          static_cast<REFERENCE_TIME>(
              10000.0 * 1000 * m_frames_in_buffer / m_format.Format.nSamplesPerSec + 0.5) +
          Config::Get(Config::MAIN_AUDIO_LATENCY) * 10000;

      result = audio_client->Initialize(
          AUDCLNT_SHAREMODE_EXCLUSIVE,
          AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST, device_period,
          device_period, reinterpret_cast<WAVEFORMATEX*>(&m_format), nullptr);
    }

    if (!HandleWinAPI("Failed to initialize IAudioClient", result))
      return false;

    result = audio_client->GetBufferSize(&m_frames_in_buffer);

    if (!HandleWinAPI("Failed to get buffer size from IAudioClient", result))
      return false;

    ComPtr<IAudioRenderClient> audio_renderer;

    result = audio_client->GetService(IID_PPV_ARGS(audio_renderer.GetAddressOf()));

    if (!HandleWinAPI("Failed to get IAudioRenderClient from IAudioClient", result))
      return false;

    wil::unique_event_nothrow need_data_event;
    need_data_event.create();

    audio_client->SetEventHandle(need_data_event.get());

    result = audio_client->Start();

    if (!HandleWinAPI("Failed to get IAudioRenderClient from IAudioClient", result))
      return false;

    INFO_LOG_FMT(AUDIO, "WASAPI: Successfully initialized!");

    // "Commit" audio client and audio renderer now
    m_audio_client = std::move(audio_client);
    m_audio_renderer = std::move(audio_renderer);
    m_need_data_event = std::move(need_data_event);

    m_running.store(true, std::memory_order_relaxed);
    m_thread = std::thread(&WASAPIStream::SoundLoop, this);
  }
  else
  {
    m_running.store(false, std::memory_order_relaxed);

    if (m_thread.joinable())
      m_thread.join();

    m_need_data_event.reset();
    m_audio_renderer.Reset();
    m_audio_client.Reset();
  }

  return true;
}

void WASAPIStream::SoundLoop()
{
  Common::SetCurrentThreadName("WASAPI Handler");
  BYTE* data;

  m_audio_renderer->GetBuffer(m_frames_in_buffer, &data);
  m_audio_renderer->ReleaseBuffer(m_frames_in_buffer, AUDCLNT_BUFFERFLAGS_SILENT);

  while (m_running.load(std::memory_order_relaxed))
  {
    WaitForSingleObject(m_need_data_event.get(), 1000);

    m_audio_renderer->GetBuffer(m_frames_in_buffer, &data);

    s16* audio_data = reinterpret_cast<s16*>(data);
    GetMixer()->Mix(audio_data, m_frames_in_buffer);

    const bool is_muted =
        Config::Get(Config::MAIN_AUDIO_MUTED) || Config::Get(Config::MAIN_AUDIO_VOLUME) == 0;
    const bool need_volume_adjustment = Config::Get(Config::MAIN_AUDIO_VOLUME) != 100 && !is_muted;

    if (need_volume_adjustment)
    {
      const float volume = Config::Get(Config::MAIN_AUDIO_VOLUME) / 100.0f;

      for (u32 i = 0; i < m_frames_in_buffer * 2; i++)
        *audio_data++ *= volume;
    }

    m_audio_renderer->ReleaseBuffer(m_frames_in_buffer, is_muted ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
  }
}

#endif  // _WIN32
