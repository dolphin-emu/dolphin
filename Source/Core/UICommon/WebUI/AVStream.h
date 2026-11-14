// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <picojson.h>

#include "Common/CommonTypes.h"
#include "Common/Functional.h"

#include "UICommon/WebUI/AVEncoder.h"

namespace WebUI
{

// This class encodes audio + video, connecting the output to many handlers.
class AVStream final
{
public:
  AVStream();
  ~AVStream();

  AVStream(AVStream&&) = default;
  AVStream& operator=(AVStream&&) = default;

  AVStream(const AVStream&) = delete;
  AVStream& operator=(const AVStream&) = delete;

  // This odd interface avoids unnecessary copying.
  void TakeAudioSamples(std::size_t frame_count,
                        Common::MoveOnlyFunction<void(std::span<float>)> fill_buffer_callback);

  void PushVideoFrame(VideoEncoder::FrameDetails frame);

  void FlushVideo();

  using PacketHandler = Common::MoveOnlyFunction<void(std::span<const u8>, s64)>;
  void AddAudioPacketHandler(std::weak_ptr<PacketHandler>);
  void AddVideoPacketHandler(std::weak_ptr<PacketHandler>);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

  std::unique_ptr<AudioEncoder> m_audio_encoder;
  std::unique_ptr<VideoEncoder> m_video_encoder;
};

}  // namespace WebUI
