// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/WASAPIStream.h"

#ifdef _WIN32

// clang-format off
#include <algorithm>
#include <Audioclient.h>
#include <cassert>
#include <comdef.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <Audiopolicy.h>
// clang-format on

#include "AudioCommon/AudioCommon.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "VideoCommon/OnScreenDisplay.h"

#define STEREO_CHANNELS 2
#define SURROUND_CHANNELS 6
// Name that the config string we read from will have if it wants the default device/endpoint to be
// used. This is better than an empty string as some devices might theoretically not have a name
#define DEFAULT_DEVICE_NAME "default"

// Helper to avoid having to call CoUninitialize() on multiple return cases
class AutoCoInit
{
public:
  AutoCoInit()
  {
    // COINIT_MULTITHREADED seems to be the better choice here given we handle multithread access
    // safely ourself and WASAPI is made for MTA. It should also perform better.
    // Note that calls to this from the main thread will return RPC_E_CHANGED_MODE
    // as some external library is calling CoInitialize with COINIT_APARTMENTTHREADED
    // without uninitializing it. We keep going
    result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // It's very rare for this to happen so we don't append the consequences of the error
    if (result != RPC_E_CHANGED_MODE && FAILED(result))
      ERROR_LOG(AUDIO, "WASAPI: Failed to initialize the COM library");
  }
  ~AutoCoInit()
  {
    if (SUCCEEDED(result))
      CoUninitialize();
  }

  bool Succeeded() const { return result == RPC_E_CHANGED_MODE || SUCCEEDED(result); }

private:
  HRESULT result;
};

// Helper to print a failed message on screen if we fail to start (and isn't retrying)
class AutoPrintMsg
{
public:
  AutoPrintMsg(WASAPIStream* WASAPI_stream) : m_WASAPI_stream(WASAPI_stream) {}
  ~AutoPrintMsg()
  {
    if (!m_WASAPI_stream->IsRunning() && !m_WASAPI_stream->ShouldRestart())
    {
      std::string message =
          "WASAPI failed to start, the current audio device likely does not support 2.0 16-bit " +
          std::to_string(m_WASAPI_stream->GetMixer()->GetSampleRate()) +
          "Hz PCM audio."
          "\nWASAPI exclusive mode (event driven) won't work";

      ERROR_LOG(AUDIO, message.c_str());
      OSD::AddMessage(message, 6000U, OSD::Color::RED);
    }
  }

private:
  WASAPIStream* m_WASAPI_stream;
};

// The implementation of MSCOM was copied from MSDN. I doubt it's needed
class CustomNotificationClient : public IMMNotificationClient
{
public:
  CustomNotificationClient(WASAPIStream* stream) : m_ref_count(1), m_stream(stream) {}

  ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&m_ref_count); }
  ULONG STDMETHODCALLTYPE Release()
  {
    ULONG ref_count = InterlockedDecrement(&m_ref_count);
    if (0 == ref_count)
    {
      delete this;
    }
    return ref_count;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface)
  {
    if (__uuidof(IUnknown) == riid)
    {
      AddRef();
      *ppvInterface = (IUnknown*)this;
    }
    else if (__uuidof(IMMNotificationClient) == riid)
    {
      AddRef();
      *ppvInterface = (IMMNotificationClient*)this;
    }
    else
    {
      *ppvInterface = nullptr;
      return E_NOINTERFACE;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR device_id)
  {
    if ((flow == EDataFlow::eRender || flow == EDataFlow::eAll) && m_stream->IsRunning() &&
        m_stream->IsUsingDefaultDevice())
    {
      // Trigger a restart so it will resume from the new default device
      m_stream->OnSettingsChanged();
    }
    return S_OK;
  }

  // We already handle these events: if a stream is running and affected, it will be restarted.
  // We probably couldn't know from here if the stream will fail to keep running,
  // so we couldn't tell whether it needs restarting or not
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR device_id) { return S_OK; };
  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR device_id) { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR device_id, DWORD new_state)
  {
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR device_id, const PROPERTYKEY key)
  {
    return S_OK;
  }

  LONG m_ref_count;
  WASAPIStream* m_stream;
};

static bool HandleWinAPI(std::string message, HRESULT result)
{
  if (result != S_OK)
  {
    _com_error err(result);
    std::string error = TStrToUTF8(err.ErrorMessage()).c_str();

    switch (result)
    {
    case AUDCLNT_E_DEVICE_IN_USE:
      error = "Audio endpoint already in use";
      break;
    case AUDCLNT_E_RESOURCES_INVALIDATED:
      error = "Audio resource invalidated (e.g. by a device removal)";
      break;
    case AUDCLNT_E_DEVICE_INVALIDATED:
      error = "Audio endpoint invalidated (e.g. by a change of settings)";
      break;
    }

    ERROR_LOG(AUDIO, "WASAPI: %s (%s)", message.c_str(), error.c_str());
  }

  return SUCCEEDED(result);
}

WASAPIStream::WASAPIStream()
{
  // Just to avoid moving AutoCoInit to the header file
  m_aci = std::make_unique<AutoCoInit>();
}

WASAPIStream::~WASAPIStream()
{
  // As long as this is wrapped around AudioCommon, this won't ever trigger
  if (m_running)
    SetRunning(false);

  if (m_enumerator)
  {
    m_enumerator->Release();
    if (m_notification_client)
    {
      // Ignore the result, we can't do much about it
      m_enumerator->UnregisterEndpointNotificationCallback(m_notification_client);
      m_notification_client->Release();
    }
  }
}

std::vector<std::string> WASAPIStream::GetAvailableDevices()
{
  AutoCoInit aci;
  if (!aci.Succeeded())
    return {};

  IMMDeviceEnumerator* enumerator = nullptr;

  HRESULT result =
      CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                       __uuidof(IMMDeviceEnumerator), reinterpret_cast<LPVOID*>(&enumerator));

  if (!HandleWinAPI("Failed to create MMDeviceEnumerator", result))
    return {};

  std::vector<std::string> device_names;
  IMMDeviceCollection* devices;
  result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);

  if (!HandleWinAPI("Failed to get available devices", result))
  {
    enumerator->Release();
    return {};
  }

  UINT count;
  devices->GetCount(&count);

  for (u32 i = 0; i < count; ++i)
  {
    IMMDevice* device;
    devices->Item(i, &device);
    if (!HandleWinAPI("Failed to get device " + std::to_string(i), result))
      continue;

    IPropertyStore* device_properties;

    result = device->OpenPropertyStore(STGM_READ, &device_properties);

    if (!HandleWinAPI("Failed to get IPropertyStore", result))
      continue;

    PROPVARIANT device_name;
    PropVariantInit(&device_name);

    device_properties->GetValue(PKEY_Device_FriendlyName, &device_name);

    device_names.push_back(TStrToUTF8(device_name.pwszVal));

    if (device_names.back() == DEFAULT_DEVICE_NAME)
    {
      WARN_LOG(AUDIO, "WASAPI: Device names called \"%s\" are not supported", DEFAULT_DEVICE_NAME);
    }

    PropVariantClear(&device_name);

    device_properties->Release();
    device_properties = nullptr;
  }

  devices->Release();
  enumerator->Release();

  return device_names;
}

IMMDevice* WASAPIStream::GetDeviceByName(const std::string& name)
{
  AutoCoInit aci;
  if (!aci.Succeeded())
    return nullptr;

  IMMDeviceEnumerator* enumerator = nullptr;

  HRESULT result =
      CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                       __uuidof(IMMDeviceEnumerator), reinterpret_cast<LPVOID*>(&enumerator));

  if (!HandleWinAPI("Failed to create MMDeviceEnumerator", result))
    return nullptr;

  IMMDeviceCollection* devices;
  result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);

  if (!HandleWinAPI("Failed to get available devices", result))
  {
    enumerator->Release();
    return {};
  }

  UINT count;
  devices->GetCount(&count);

  for (u32 i = 0; i < count; ++i)
  {
    IMMDevice* device;
    devices->Item(i, &device);
    if (!HandleWinAPI("Failed to get device " + std::to_string(i), result))
      continue;

    IPropertyStore* device_properties;

    result = device->OpenPropertyStore(STGM_READ, &device_properties);

    if (!HandleWinAPI("Failed to get IPropertyStore", result))
      continue;

    PROPVARIANT device_name;
    PropVariantInit(&device_name);

    device_properties->GetValue(PKEY_Device_FriendlyName, &device_name);

    device_properties->Release();
    device_properties = nullptr;

    if (TStrToUTF8(device_name.pwszVal) == name)
    {
      PropVariantClear(&device_name);
      devices->Release();
      enumerator->Release();
      return device;
    }

    PropVariantClear(&device_name);
  }

  devices->Release();
  enumerator->Release();

  return nullptr;
}

std::vector<unsigned long> WASAPIStream::GetSelectedDeviceSampleRates()
{
  std::vector<unsigned long> device_sample_rates;

  AutoCoInit aci;
  if (!aci.Succeeded())
    return device_sample_rates;

  IMMDeviceEnumerator* enumerator = nullptr;
  IMMDevice* device = nullptr;
  IAudioClient* audio_client = nullptr;

  HRESULT result =
      CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                       __uuidof(IMMDeviceEnumerator), reinterpret_cast<LPVOID*>(&enumerator));

  if (!HandleWinAPI("Failed to create MMDeviceEnumerator", result))
    return device_sample_rates;

  if (SConfig::GetInstance().sWASAPIDevice == DEFAULT_DEVICE_NAME)
  {
    result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
  }
  else
  {
    device = GetDeviceByName(SConfig::GetInstance().sWASAPIDevice);
    result = S_OK;

    if (!device)
    {
      ERROR_LOG(AUDIO, "WASAPI: Can't find device '%s', falling back to default",
                SConfig::GetInstance().sWASAPIDevice.c_str());
      result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    }
  }

  if (!HandleWinAPI("Failed to get default endpoint", result))
  {
    enumerator->Release();
    return device_sample_rates;
  }

  // Note from Microsoft: In Win 8, the first use of IAudioClient to access the audio device
  // should be on the STA thread. Calls from an MTA thread may result in undefined behavior.
  // We haven't implemented that, but cubeb said they never found the problem, but it could be
  // exclusive to exclisve mode, or it could have been patched in newer Win 8 versions
  result = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
                            reinterpret_cast<LPVOID*>(&audio_client));

  if (!HandleWinAPI("Failed to reactivate IAudioClient", result))
  {
    device->Release();
    enumerator->Release();
    return device_sample_rates;
  }

  // Anything below 48kHz should not be used, but I've added 44.1kHz in case
  // a device doesn't support 48kHz. Any new value can be added here
  unsigned long worst_to_best_sample_rates[5] = {44100, 48000, 96000, 192000, 384000};
  s32 i = ARRAYSIZE(worst_to_best_sample_rates) - 1;

  WAVEFORMATEXTENSIBLE format;
  format.Format.nChannels = STEREO_CHANNELS;
  format.Format.wBitsPerSample = 16;
  format.Format.nBlockAlign = format.Format.nChannels * format.Format.wBitsPerSample / 8;
  format.Samples.wValidBitsPerSample = format.Format.wBitsPerSample;
  format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
  format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

  while (i >= 0)
  {
    format.Format.nSamplesPerSec = worst_to_best_sample_rates[i];
    format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec * format.Format.nBlockAlign;

    format.Format.wFormatTag = WAVE_FORMAT_PCM;
    format.Format.cbSize = 0;
    // Microsoft specified this needs to be called twice, once with WAVEFORMATEX
    // and once with WAVEFORMATEXTENSIBLE
    result = audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                             (const WAVEFORMATEX*)&format, nullptr);

    if (!SUCCEEDED(result))
    {
      --i;
      continue;
    }

    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    result = audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                             (const WAVEFORMATEX*)&format, nullptr);

    if (!SUCCEEDED(result))
    {
      --i;
      continue;
    }

    device_sample_rates.push_back(worst_to_best_sample_rates[i]);
    --i;
  }

  HandleWinAPI("Found no supported device formats, will default to " +
               std::to_string(AudioCommon::GetDefaultSampleRate()) +
               " whether supported or not", result);

  device->Release();
  audio_client->Release();
  enumerator->Release();

  return device_sample_rates;
}

bool WASAPIStream::SetRestartFromResult(HRESULT result)
{
  // This can be triggered by changing the "Spacial sound" settings or the sample rate
  if (result == AUDCLNT_E_DEVICE_INVALIDATED || result == AUDCLNT_E_RESOURCES_INVALIDATED)
  {
    WARN_LOG(AUDIO, "WASAPI: The audio device has either been removed or a setting has recently "
                    "changed and Windows is recomputing it. Restarting WASAPI");
    // Doing this here is definitely a hack, the correct way would be to use IMMNotificationClient
    // but it seems more complicated to use, relying on errors is just simpler
    m_should_restart = true;
    return true;
  }

  return false;
}

bool WASAPIStream::Init()
{
  // Both CLSCTX_INPROC_SERVER or CLSCTX_ALL should be fine for Dolphin
  HRESULT result =
      CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                       __uuidof(IMMDeviceEnumerator), reinterpret_cast<LPVOID*>(&m_enumerator));

  if (!HandleWinAPI("Failed to create MMDeviceEnumerator", result))
    return false;

  m_notification_client = new CustomNotificationClient(this);
  result = m_enumerator->RegisterEndpointNotificationCallback(m_notification_client);
  if (FAILED(result))
  {
    m_notification_client->Release();
    m_notification_client = nullptr;
  }

  return true;
}

bool WASAPIStream::SetRunning(bool running)
{
  assert(running != m_running);

  // Can be called by multiple threads (main and emulation), we need to initialize COM
  AutoCoInit aci;
  if (!aci.Succeeded())
    return false;

  m_should_restart = false;

  if (running)
  {
    unsigned long sample_rate = AudioCommon::GetDefaultSampleRate();

    // Make sure the selected mode is still supported (ignored if we are using the default device)
    char* pEnd = nullptr;
    unsigned long user_preferred_sample_rate =
        std::strtoull(SConfig::GetInstance().sWASAPIDeviceSampleRate.c_str(), &pEnd, 10);
    if (SConfig::GetInstance().sWASAPIDevice != DEFAULT_DEVICE_NAME && pEnd != nullptr &&
        user_preferred_sample_rate != 0)
    {
      std::vector<unsigned long> selected_device_sample_rates =
          WASAPIStream::GetSelectedDeviceSampleRates();
      for (unsigned long selected_device_sample_rate : selected_device_sample_rates)
      {
        if (selected_device_sample_rate == user_preferred_sample_rate)
        {
          sample_rate = user_preferred_sample_rate;
          break;
        }
      }
    }

    // Recreate the mixer with our preferred sample rate
    GetMixer()->UpdateSettings(sample_rate);

    m_surround = SConfig::GetInstance().ShouldUseDPL2Decoder();
    m_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    m_format.Format.nChannels = m_surround ? SURROUND_CHANNELS : STEREO_CHANNELS;
    m_format.Format.nSamplesPerSec = GetMixer()->GetSampleRate();
    // We could use 24 bit (in 32 bit containers) when using surround, as the surround mixer gives
    // us a float between -1 and 1, but it was originally mixed from 16 bit source anyway, so there
    // likely won't be any loss in quality. The other advantage would be that we wouldn't need to
    // use an intermediary array as we could just use the surround array, but again,
    // that's not worth it. The only advantage would be when the volume isn't set to 100%
    m_format.Format.wBitsPerSample = 16;
    m_format.Format.nBlockAlign = m_format.Format.nChannels * m_format.Format.wBitsPerSample / 8;
    m_format.Format.nAvgBytesPerSec = m_format.Format.nSamplesPerSec * m_format.Format.nBlockAlign;
    m_format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    m_format.Samples.wValidBitsPerSample = m_format.Format.wBitsPerSample;
    m_format.dwChannelMask = m_surround ? (SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT   |
                                           SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                                           SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT   ):
                                          (SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  );
    // Some sound cards might support KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, which would avoid a
    // conversion when using DPLII, though I didn't think it was worth implementing
    m_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    IMMDevice* device = nullptr;

    AutoPrintMsg apm(this);

    HRESULT result;

    if (SConfig::GetInstance().sWASAPIDevice == DEFAULT_DEVICE_NAME)
    {
      result = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
      m_using_default_device = true;
    }
    else
    {
      device = GetDeviceByName(SConfig::GetInstance().sWASAPIDevice);
      result = S_OK;
      m_using_default_device = false;

      // Whether this is the first time WASAPI starts running or not,
      // we will fallback on the default device if the custom one is not found.
      // This might happen if the device got removed (WASAPI would trigger m_should_restart).
      // An alternative would be to NOT fallback on the default device if a custom one
      // is specified, especially in case the specified one was previously available but then
      // just got disconnected, but overall, this should be fine
      if (!device)
      {
        ERROR_LOG(AUDIO, "WASAPI: Can't find device '%s', falling back to default",
                  SConfig::GetInstance().sWASAPIDevice.c_str());
        result = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        m_using_default_device = true;
      }
    }

    if (!HandleWinAPI("Failed to get default endpoint", result))
      return false;

    // Show a friendly name in the log (no other use)
    IPropertyStore* device_properties;
    result = device->OpenPropertyStore(STGM_READ, &device_properties);

    if (HandleWinAPI("Failed to get IPropertyStore", result))
    {
      PROPVARIANT device_name;
      PropVariantInit(&device_name);

      device_properties->GetValue(PKEY_Device_FriendlyName, &device_name);

      INFO_LOG(AUDIO, "WASAPI: Using Audio Device '%s'", TStrToUTF8(device_name.pwszVal).c_str());

      PropVariantClear(&device_name);

      device_properties->Release();
      device_properties = nullptr;
    }

    // TODO: Microsoft says we need to create and destroy IAudioClient and all the pointers we get
    // from it in the same thread. Unfortunately, due to the way Dolphin is done, this isn't easy,
    // as the audio backend can be started and stopped (SetRunning()) from the main and emu threads.
    // We can't move all of this between Init() and the destructor as if we paused the game, WASAPI
    // would keep exclusive access of the device. I've also tried to move all of this in SoundLoop()
    // (m_thread), but m_need_data_event failed to trigger.
    // Let's just hope it works fine without it, at worse it will cause a small leak, but this
    // has always been the case.
    // We don't need IAudioClient3, it doesn't add anything for exclusive mode.
    result = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
                              reinterpret_cast<LPVOID*>(&m_audio_client));

    if (SetRestartFromResult(result) || !HandleWinAPI("Failed to activate IAudioClient", result))
    {
      device->Release();
      return false;
    }
    
    // Clamp by the minimum supported device period (latency).
    // Microsoft said it would automatically be clamped but setting
    // device_period to 1 or 0 failed the IAudioClient::Initialize.
    // WASAPI exclusive mode supports up to 5000ms, but let's clamp it,
    // to 500ms and the mixer buffer length
    REFERENCE_TIME device_period = 0;
    result = m_audio_client->GetDevicePeriod(nullptr, &device_period);
    device_period = std::clamp((REFERENCE_TIME)AudioCommon::GetUserTargetLatency() * 10000,
                               device_period, (REFERENCE_TIME)500 * 10000);

    if (SetRestartFromResult(result) || !HandleWinAPI("Failed to get device period", result))
    {
      device->Release();
      m_audio_client->Release();
      m_audio_client = nullptr;
      return false;
    }

    DWORD stream_flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

    // We don't need a GUID as we only have one audio session at a time
    result = m_audio_client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, stream_flags, device_period,
                                        device_period, &m_format.Format, nullptr);

    // Fallback to stereo if surround 5.1 is not supported
    if (m_surround && result == AUDCLNT_E_UNSUPPORTED_FORMAT)
    {
      WARN_LOG(AUDIO, "WASAPI: Your current audio device doesn't support 5.1 16-bit %uHz"
                      " PCM audio. Will fallback to 2.0 16-bit",
                        GetMixer()->GetSampleRate());

      m_surround = false;
      m_format.Format.nChannels = STEREO_CHANNELS;
      m_format.Format.nBlockAlign = m_format.Format.nChannels * m_format.Format.wBitsPerSample / 8;
      m_format.Format.nAvgBytesPerSec =
          m_format.Format.nSamplesPerSec * m_format.Format.nBlockAlign;
      m_format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
      
      result = m_audio_client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, stream_flags, device_period,
                                          device_period, &m_format.Format, nullptr);
    }
    // TODO: just like we fallback here above if we don't support surround, we should add support for
    // more devices (e.g. bitrate, channels, format) by falling back as well

    if (result == AUDCLNT_E_UNSUPPORTED_FORMAT)
    {
      WARN_LOG(AUDIO, "WASAPI: Your current audio device doesn't support 2.0 16-bit %uHz"
                      " PCM audio. Exclusive mode (event driven) won't work",
                        GetMixer()->GetSampleRate());

      device->Release();
      m_audio_client->Release();
      m_audio_client = nullptr;
      return false;
    }

    // Standard procedure from Microsoft documentation in case we receive this error
    if (result == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
      result = m_audio_client->GetBufferSize(&m_frames_in_buffer);
      m_audio_client->Release();
      m_audio_client = nullptr;

      if (SetRestartFromResult(result) ||
          !HandleWinAPI("Failed to get buffer size from IAudioClient", result))
      {
        device->Release();
        return false;
      }

      result = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr,
                                reinterpret_cast<LPVOID*>(&m_audio_client));

      if (SetRestartFromResult(result) ||
          !HandleWinAPI("Failed to reactivate IAudioClient", result))
      {
        device->Release();
        return false;
      }

      WARN_LOG(AUDIO, "WASAPI: Your current audio device doesn't support the latency (period)"
                      " you specified. Falling back to default device latency");

      // m_frames_in_buffer should already "include" our previously attempted latency,
      // so we don't need to multiply by our target latency again
      device_period = (REFERENCE_TIME)(
          (10000.0 * 1000 / m_format.Format.nSamplesPerSec * m_frames_in_buffer) + 0.5);

      // This should always work so we don't need to warn the user that WASAPI failed again
      result = m_audio_client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, stream_flags, device_period,
                                          device_period, &m_format.Format, nullptr);
    }

    // Show this even if we failed, just for information to the user
    INFO_LOG(AUDIO, "WASAPI: Sample Rate: %u, Channels: %u, Audio Period: %ims",
             m_format.Format.nSamplesPerSec, m_format.Format.nChannels, device_period / 10000);

    if (SetRestartFromResult(result) || !HandleWinAPI("Failed to initialize IAudioClient", result))
    {
      device->Release();
      m_audio_client->Release();
      m_audio_client = nullptr;
      return false;
    }

    result = m_audio_client->GetBufferSize(&m_frames_in_buffer);

    if (SetRestartFromResult(result) ||
        !HandleWinAPI("Failed to get buffer size from IAudioClient", result))
    {
      device->Release();
      m_audio_client->Release();
      m_audio_client = nullptr;
      return false;
    }

    result = m_audio_client->GetService(__uuidof(IAudioRenderClient),
                                        reinterpret_cast<LPVOID*>(&m_audio_renderer));

    if (SetRestartFromResult(result) ||
        !HandleWinAPI("Failed to get IAudioRenderClient from IAudioClient", result))
    {
      device->Release();
      m_audio_client->Release();
      m_audio_client = nullptr;
      return false;
    }

    m_need_data_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_audio_client->SetEventHandle(m_need_data_event);

    result = m_audio_client->Start();

    if (SetRestartFromResult(result) || !HandleWinAPI("Failed to start IAudioClient", result))
    {
      device->Release();
      m_audio_renderer->Release();
      m_audio_renderer = nullptr;
      m_audio_client->Release();
      m_audio_client = nullptr;
      CloseHandle(m_need_data_event);
      m_need_data_event = nullptr;
      return false;
    }

    // We don't really care if this fails, we only use this to make sure the we didn't miss any
    // output periods, but if we can't guarantee that WASAPI will work anyway
    result = m_audio_client->GetService(__uuidof(IAudioClock),
                                        reinterpret_cast<LPVOID*>(&m_audio_clock));
    if (SUCCEEDED(result))
    {
      // This should always be identical to m_format.Format.nSamplesPerSec, but I can't guarantee
      u64 device_frequency = 0;
      m_audio_clock->GetFrequency(&device_frequency);

      if (device_frequency != m_format.Format.nSamplesPerSec)
      {
        // m_audio_clock->GetPosition would return unreliable values
        WARN_LOG(AUDIO, "WASAPI: The device frequence is different from our sample rate, we can't"
                        " keep them in sync. WASAPI will still work");
        m_audio_clock->Release();
        m_audio_clock = nullptr;
      }
    }

    // Again, if this fails, we can go on, we just won't know the volume of the application
    // the user has set through the Windows mixer
    IAudioSessionManager* audio_session_manager = nullptr;
    result = device->Activate(__uuidof(IAudioSessionManager), CLSCTX_INPROC_SERVER, nullptr,
                              reinterpret_cast<LPVOID*>(&audio_session_manager));
    if (SUCCEEDED(result))
    {
      result = audio_session_manager->GetSimpleAudioVolume(nullptr, false, &m_simple_audio_volume);
      audio_session_manager->Release();
    }

    m_running = true;

    device->Release();

    NOTICE_LOG(AUDIO, "WASAPI: Successfully initialized");

    m_stop_thread_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    m_thread = std::thread([this] { SoundLoop(); });
  }
  else
  {
    m_running = false;

    SetEvent(m_stop_thread_event);

    m_thread.join();

    m_surround = false;
    m_using_default_device = false;

    // As long as we don't call SetRunning(false) while m_running was already false,
    // there is no need to check pointers here
    m_audio_renderer->Release();
    m_audio_renderer = nullptr;
    m_audio_client->Release();
    m_audio_client = nullptr;
    if (m_audio_clock)
    {
      m_audio_clock->Release();
      m_audio_clock = nullptr;
    }
    if (m_simple_audio_volume)
    {
      m_simple_audio_volume->Release();
      m_simple_audio_volume = nullptr;
    }
    CloseHandle(m_need_data_event);
    m_need_data_event = nullptr;
    CloseHandle(m_stop_thread_event);
    m_stop_thread_event = nullptr;
  }

  return true;
}

void WASAPIStream::Update()
{
  // TODO: move this out of the game (main) thread, restarting WASAPI should not lock the game
  // thread, but as of now there isn't any other constant and safe access point to restart

  // If the sound loop failed for some reason, re-initialize ASAPI to resume playback
  if (m_should_restart)
  {
    m_should_restart = false;
    if (m_running)
    {
      // We need to pass through AudioCommon as it has a mutex and
      // to make sure s_sound_stream_running is updated
      if (AudioCommon::SetSoundStreamRunning(false, false))
      {
        // m_should_restart is triggered when the device is currently
        // invalidated, and it will stay for a while, so this new call
        // to SetRunning(true) might fail, but if it fails some
        // specific reasons, it will set m_should_restart true again.
        // A Sleep(10) call also seemed to fix the problem but it's hacky.
        AudioCommon::SetSoundStreamRunning(true, false);
      }
    }
    else
    {
      AudioCommon::SetSoundStreamRunning(true, false);
    }
  }
}

void WASAPIStream::SoundLoop()
{
  Common::SetCurrentThreadName("WASAPI Handler");
  BYTE* data;

  // New thread, we need to initialize COM
  AutoCoInit aci;
  if (!aci.Succeeded())
  {
    m_should_restart = true;
    return;
  }

  // I'm not sure what this is for, but it doesn't work without it. Maybe to init thread access
  if (SUCCEEDED(m_audio_renderer->GetBuffer(m_frames_in_buffer, &data)))
  {
    m_audio_renderer->ReleaseBuffer(m_frames_in_buffer, AUDCLNT_BUFFERFLAGS_SILENT);
  }

  u64 submitted_samples = 0;

  while (true)
  {
    // "stop" has greater priority than "data" event (if both triggered at once)
    HANDLE handles[2] = {m_stop_thread_event, m_need_data_event};
    DWORD r = WaitForMultipleObjects(2, handles, false, INFINITE);

    if (r == WAIT_OBJECT_0)
    {
      break;
    }
    else if (r != WAIT_OBJECT_0 + 1)
    {
      // We don't set m_should_restart to true here because
      // we can't guarantee it wouldn't constantly fail again
      ERROR_LOG(AUDIO, "WASAPI: WaitForMultipleObjects failed. Stopping sound playback."
                       " Pausing and unpausing the emulation might fix the problem");
      break;
    }

    // We could as well override the SoundStream::SetVolume(int) function
    float volume = SConfig::GetInstance().m_IsMuted ? 0.f : SConfig::GetInstance().m_Volume / 100.f;
    if (m_simple_audio_volume)
    {
      float master_volume = 1.f;
      BOOL mute = false;
      // Ignore them in case they fail
      m_simple_audio_volume->GetMasterVolume(&master_volume);
      m_simple_audio_volume->GetMute(&mute);
      volume *= mute ? 0.f : master_volume;
    }

    HRESULT result = m_audio_renderer->GetBuffer(m_frames_in_buffer, &data);

    if (SetRestartFromResult(result) ||
        !HandleWinAPI("Failed to get buffer from IAudioClient. Stopping sound playback."
                      " Pausing and unpausing the emulation might fix the problem", result))
    {
      // We don't set m_should_restart to true in the second case here
      // because we can't guarantee it wouldn't constantly fail again
      break;
    }

    if (m_surround)
    {
      m_surround_samples.resize(m_frames_in_buffer * SURROUND_CHANNELS);
      unsigned int mixed_samples =
          GetMixer()->MixSurround(m_surround_samples.data(), m_frames_in_buffer);

      // From float(32) (~-1 to ~1 but can go beyond limits) to signed short(16)
      for (u32 i = 0; i < mixed_samples * SURROUND_CHANNELS; ++i)
      {
        m_surround_samples[i] *= volume * std::numeric_limits<s16>::max();
        m_surround_samples[i] =
            std::clamp(m_surround_samples[i], float(std::numeric_limits<s16>::min()),
                       float(std::numeric_limits<s16>::max()));

        // We assume the volume won't be more than 1, or we'll get some clipping. It would be better
        // to round the result, but the chance of that making a difference is not worth the
        // performance hit
        reinterpret_cast<s16*>(data)[i] = static_cast<s16>(m_surround_samples[i]);
      }
    }
    // From signed short(16) to signed short(16)
    else
    {
      GetMixer()->Mix(reinterpret_cast<s16*>(data), m_frames_in_buffer);
      
      for (u32 i = 0; i < m_frames_in_buffer * STEREO_CHANNELS; ++i)
      {
        reinterpret_cast<s16*>(data)[i] =
            static_cast<s16>(reinterpret_cast<s16*>(data)[i] * volume);
      }
    }

    // If the device period was set too low, the above computations might take longer
    // that the latency, so we'd lose this audio frame
    m_audio_renderer->ReleaseBuffer(m_frames_in_buffer, 0);
    submitted_samples += m_frames_in_buffer;
    m_wait_next_desync_check -= m_frames_in_buffer;

    // When pausing and unpausing the process (e.g. a breakpoint) or toggling fullscreen
    // WASAPI would fall out of sync without letting us know, and start outputting frames
    // that had not been calculated yet, so garbage, thus crackling. This seems to be
    // a simple and effective fix (not sure they are any official/better fixes)
    if (m_audio_clock && m_wait_next_desync_check <= 0)
    {
      u64 device_samples_position;
      // This should roughly return the "sample" the audio device is currently playing,
      // if it's playing the last one we submitted already, then something is wrong,
      // we likely have missed a period, so we'd better restart WASAPI
      result = m_audio_clock->GetPosition(&device_samples_position, nullptr);

      if (SUCCEEDED(result) && device_samples_position >= submitted_samples)
      {
        WARN_LOG(AUDIO, "WASAPI: Fell out of sync with the device period as we could not compute"
                        " samples in time. "
                        "Restarting WASAPI to avoid crackling. If this keeps happening,"
                        " increase your backend latency");

        // Don't check more than roughly once per second, as this could trigger a lot otherwise
        m_wait_next_desync_check = m_format.Format.nSamplesPerSec;

        m_should_restart = true;
        break;
      }
    }
  }
}

#endif  // _WIN32
