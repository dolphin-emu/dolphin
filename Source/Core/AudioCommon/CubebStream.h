// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

#include "AudioCommon/SoundStream.h"

#include <cubeb/cubeb.h>

class CubebStream final : public SoundStream
{
public:
  ~CubebStream() override;
  bool Init() override;
  bool SetRunning(bool running) override;
  void Update() override;
  void SetVolume(int) override;

  bool SupportsRuntimeSettingsChanges() const override { return true; }
  void OnSettingsChanged() override
  {
    m_settings_changed = true;
    m_should_restart = true;
  }

private:
  bool m_stereo = false;
  std::shared_ptr<cubeb> m_ctx;
  cubeb_stream* m_stream = nullptr;

  std::vector<short> m_short_buffer;
  std::vector<float> m_floatstereo_buffer;

  std::atomic<bool> m_running = false;
  std::atomic<bool> m_should_restart = false;
  std::atomic<bool> m_settings_changed = false;

  static long DataCallback(cubeb_stream* stream, void* user_data, const void* /*input_buffer*/,
                           void* output_buffer, long num_frames);
  static void StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state);
};
