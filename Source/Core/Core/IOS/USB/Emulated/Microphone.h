// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <mutex>

#include "Common/CommonTypes.h"
#include "Common/WorkQueueThread.h"

#ifdef HAVE_CUBEB
#include "AudioCommon/CubebUtils.h"

struct cubeb;
struct cubeb_stream;
#endif

namespace IOS::HLE::USB
{
struct WiiSpeakState;

class Microphone final
{
public:
  Microphone(const WiiSpeakState& sampler);
  ~Microphone();

  bool HasData(u32 sample_count) const;
  void ReadIntoBuffer(u8* dst, u32 size);

private:
#ifdef HAVE_CUBEB
  static long CubebDataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                                void* output_buffer, long nframes);
#endif

  long DataCallback(const s16* input_buffer, long nframes);

  void StreamInit();
  void StreamTerminate();
  void StreamStart();
  void StreamStop();

  static constexpr u32 SAMPLING_RATE = 8000;
  using SampleType = s16;
  static constexpr u32 BUFF_SIZE_SAMPLES = 16;
  static constexpr u32 STREAM_SIZE = BUFF_SIZE_SAMPLES * 500;

  std::array<SampleType, STREAM_SIZE> m_stream_buffer{};
  u32 m_stream_wpos = 0;
  u32 m_stream_rpos = 0;
  u32 m_samples_avail = 0;

  mutable std::mutex m_ring_lock;

  const WiiSpeakState& m_sampler;

#ifdef HAVE_CUBEB
  std::shared_ptr<cubeb> m_cubeb_ctx = nullptr;
  cubeb_stream* m_cubeb_stream = nullptr;

#ifdef _WIN32
  Common::AsyncWorkThread m_work_queue{"Wii Speak Worker"};
  bool m_coinit_success = false;
  bool m_should_couninit = false;
#endif
#endif
};
}  // namespace IOS::HLE::USB
