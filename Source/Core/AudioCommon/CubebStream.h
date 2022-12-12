// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

#include "AudioCommon/SoundStream.h"
#include "Common/WorkQueueThread.h"

#include <cubeb/cubeb.h>

class CubebStream final : public SoundStream
{
public:
  CubebStream();
  CubebStream(const CubebStream& other) = delete;
  CubebStream(CubebStream&& other) = delete;
  CubebStream& operator=(const CubebStream& other) = delete;
  CubebStream& operator=(CubebStream&& other) = delete;
  ~CubebStream() override;
  bool Init() override;
  bool SetRunning(bool running) override;
  void SetVolume(int) override;

private:
  bool m_stereo = false;
  std::shared_ptr<cubeb> m_ctx;
  cubeb_stream* m_stream = nullptr;

  std::vector<short> m_short_buffer;
  std::vector<float> m_floatstereo_buffer;

#ifdef _WIN32
  Common::WorkQueueThread<std::function<void()>> m_work_queue;
  bool m_coinit_success = false;
  bool m_should_couninit = false;
#endif

  static long DataCallback(cubeb_stream* stream, void* user_data, const void* /*input_buffer*/,
                           void* output_buffer, long num_frames);
  static void StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state);
};
