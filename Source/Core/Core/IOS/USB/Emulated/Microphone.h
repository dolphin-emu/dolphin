// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <mutex>

#include "Common/CommonTypes.h"
#include "Common/WorkQueueThread.h"

struct cubeb;
struct cubeb_stream;

namespace IOS::HLE::USB
{
class Microphone final
{
public:
  Microphone();
  ~Microphone();

  bool HasData() const;
  void ReadIntoBuffer(u8* dst, u32 size);

private:
  static long DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                           void* output_buffer, long nframes);

  void StreamInit();
  void StreamTerminate();
  void StreamStart();
  void StopStream();

  static constexpr u32 SAMPLING_RATE = 8000;
  static constexpr u32 BUFFER_SIZE = SAMPLING_RATE / 2;
  static constexpr u32 BUFF_SIZE_SAMPLES = 16;
  static constexpr u32 STREAM_SIZE = BUFF_SIZE_SAMPLES * 500;

  std::array<s16, STREAM_SIZE> m_stream_buffer{};
  u32 m_stream_wpos = 0;
  u32 m_stream_rpos = 0;
  u32 m_samples_avail = 0;

  std::mutex m_ring_lock;
  std::shared_ptr<cubeb> m_cubeb_ctx = nullptr;
  cubeb_stream* m_cubeb_stream = nullptr;

#ifdef _WIN32
  Common::WorkQueueThread<std::function<void()>> m_work_queue{
      "Wii Speak Worker", [](const std::function<void()>& func) { func(); }};
  bool m_coinit_success = false;
  bool m_should_couninit = false;
#endif
};
}  // namespace IOS::HLE::USB
