// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WIN32

// clang-format off
#include <Windows.h>
#include <mmreg.h>
#include <objbase.h>
#include <wil/resource.h>
// clang-format on

#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <wrl/client.h>

#include "AudioCommon/SoundStream.h"

struct IAudioClient;
struct IAudioRenderClient;
struct IMMDevice;
struct IMMDeviceEnumerator;

#endif

class WASAPIStream final : public SoundStream
{
#ifdef _WIN32
public:
  explicit WASAPIStream();
  ~WASAPIStream();
  bool Init() override;
  bool SetRunning(bool running) override;

  static bool IsValid();
  static std::vector<std::string> GetAvailableDevices();
  static Microsoft::WRL::ComPtr<IMMDevice> GetDeviceByName(std::string_view name);

private:
  void SoundLoop();

  u32 m_frames_in_buffer = 0;
  std::atomic<bool> m_running = false;
  std::thread m_thread;

  // CoUninitialize must be called after all WASAPI COM objects have been destroyed,
  // therefore this member must be located before them, as first class fields are destructed last
  wil::unique_couninitialize_call m_coinitialize{false};

  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_enumerator;
  Microsoft::WRL::ComPtr<IAudioClient> m_audio_client;
  Microsoft::WRL::ComPtr<IAudioRenderClient> m_audio_renderer;
  wil::unique_event_nothrow m_need_data_event;
  WAVEFORMATEXTENSIBLE m_format;
#endif  // _WIN32
};
