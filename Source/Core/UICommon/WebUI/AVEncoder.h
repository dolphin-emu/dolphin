// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mdspan>
#include <span>

#include "Common/CommonTypes.h"
#include "Common/Functional.h"

namespace WebUI
{

// Matches Dolphin's Mixer.
constexpr int AUDIO_SAMPLE_RATE = 48'000;

class AVPacketProducer
{
public:
  virtual ~AVPacketProducer() = 0;

  using ReceivePacketCallbackType =
      Common::MoveOnlyFunction<void(std::span<const u8> data, s64 pts)>;

  // FYI: The callback will be invoked from a worker thread.
  void SetPacketCallback(ReceivePacketCallbackType&& callback);

  // Signal the context to drain if needed. Otherwise no-op.
  virtual void Flush() = 0;

protected:
  ReceivePacketCallbackType m_packet_callback;
};

class AudioEncoder : public AVPacketProducer
{
public:
  // This odd interface avoids unnecessary copying.
  virtual void
  TakeAudioSamples(std::size_t frame_count,
                   Common::MoveOnlyFunction<void(std::span<float>)> fill_buffer_callback) = 0;
};

class VideoEncoder : public AVPacketProducer
{
public:
  struct FrameDetails
  {
    // RGB0 data.
    std::mdspan<const u8, std::dextents<int, 2>> mdspan;
  };

  virtual void PushVideoFrame(FrameDetails frame) = 0;
};

std::unique_ptr<AudioEncoder> CreateOpusEncoder();
std::unique_ptr<VideoEncoder> CreateH264Encoder();

}  // namespace WebUI
