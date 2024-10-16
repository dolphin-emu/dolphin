// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <mutex>

#include "AudioCommon/CubebUtils.h"
#include "Common/CommonTypes.h"

struct cubeb;
struct cubeb_stream;

namespace IOS::HLE::USB
{
struct WiiSpeakState;

class Microphone final
{
public:
  Microphone(const WiiSpeakState& sampler);
  ~Microphone();

  bool HasData() const;
  u16 ReadIntoBuffer(u8* ptr, u32 size);
  const WiiSpeakState& GetSampler() const;

private:
  static long DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                           void* output_buffer, long nframes);

  void StreamInit();
  void StreamTerminate();
  void StreamStart();
  void StopStream();

  static constexpr u32 SAMPLING_RATE = 8000;
  using SampleType = s16;
  static constexpr u32 BUFF_SIZE_SAMPLES = 16;
  static constexpr u32 STREAM_SIZE = BUFF_SIZE_SAMPLES * 500;

  std::array<SampleType, STREAM_SIZE> m_stream_buffer{};
  u32 m_stream_wpos = 0;
  u32 m_stream_rpos = 0;
  u32 m_samples_avail = 0;

  std::mutex m_ring_lock;
  std::shared_ptr<cubeb> m_cubeb_ctx = nullptr;
  cubeb_stream* m_cubeb_stream = nullptr;

  const WiiSpeakState& m_sampler;

  CubebUtils::CoInitSyncWorker m_worker{"Wii Speak Worker"};
};
}  // namespace IOS::HLE::USB
