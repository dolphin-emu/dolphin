// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
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

  void StreamInit();
  void StreamTerminate();
  void ReadIntoBuffer(u8* dst, u32 size);
  bool HasData() const;

private:
  static long DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                           void* output_buffer, long nframes);

  void StreamStart();
  void StopStream();

  static constexpr u32 SAMPLING_RATE = 8000;
  static constexpr u32 BUFFER_SIZE = SAMPLING_RATE / 2;

  s16* stream_buffer;
  std::mutex ring_lock;
  std::shared_ptr<cubeb> m_cubeb_ctx = nullptr;
  cubeb_stream* m_cubeb_stream = nullptr;
  std::vector<u8> m_temp_buffer{};

  int stream_size;
  int stream_wpos;
  int stream_rpos;
  int samples_avail;
  int buff_size_samples = 16;

#ifdef _WIN32
  Common::WorkQueueThread<std::function<void()>> m_work_queue;
  bool m_coinit_success = false;
  bool m_should_couninit = false;
#endif
};
}  // namespace IOS::HLE::USB
