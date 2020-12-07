// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32
#include <Windows.h>
#include <mmreg.h>
#endif

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "AudioCommon/SoundStream.h"

struct IAudioClient;
struct IAudioRenderClient;
struct IAudioClock;
struct ISimpleAudioVolume;
struct IMMDevice;
struct IMMDeviceEnumerator;
class CustomNotificationClient;
class AutoCoInit;

class WASAPIStream final : public SoundStream
{
#ifdef _WIN32
public:
  explicit WASAPIStream();
  ~WASAPIStream();
  bool Init() override;
  bool SetRunning(bool running) override;
  void Update() override;
  void SoundLoop() override;

  bool IsRunning() const { return m_running; }
  bool ShouldRestart() const { return m_should_restart; }
  bool IsUsingDefaultDevice() const { return m_using_default_device; }

  bool SupportsRuntimeSettingsChanges() const override { return true; }
  bool IsSurroundEnabled() const override { return m_surround; }
  void OnSettingsChanged() override { m_should_restart = true; }

  static bool IsValid() { return true; }
  static std::vector<std::string> GetAvailableDevices();
  static IMMDevice* GetDeviceByName(const std::string& name);
  // Returns the user selected device supported sample rates at 16 bit and 2 channels,
  // so it ignores 24 bit or support for 5 channels. If we are starting up WASAPI with DPL2 on,
  // it will try these sample rates anyway, or fallback to the dolphin default one,
  // but generally if a device supports a sample rate, it supports it with any bitrate/channels.
  // It doesn't just return any device supported sample rate, just the ones we care for in Dolphin
  static std::vector<unsigned long> GetSelectedDeviceSampleRates();

private:
  bool SetRestartFromResult(HRESULT result);

  u32 m_frames_in_buffer = 0;
  std::atomic<bool> m_running = false;
  std::atomic<bool> m_should_restart = false;
  std::thread m_thread;

  IAudioClient* m_audio_client = nullptr;
  IAudioRenderClient* m_audio_renderer = nullptr;
  IAudioClock* m_audio_clock = nullptr;
  ISimpleAudioVolume* m_simple_audio_volume = nullptr;
  IMMDeviceEnumerator* m_enumerator = nullptr;
  HANDLE m_need_data_event = nullptr;
  HANDLE m_stop_thread_event = nullptr;
  WAVEFORMATEXTENSIBLE m_format;
  std::unique_ptr<AutoCoInit> m_aci;
  CustomNotificationClient* m_notification_client = nullptr;
  bool m_surround = false;
  bool m_using_default_device = false;
  std::vector<float> m_surround_samples;
  s64 m_wait_next_desync_check = 0;
#endif  // _WIN32
};
