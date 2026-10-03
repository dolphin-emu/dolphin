// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <picojson.h>

#include "Common/CommonTypes.h"
#include "Common/Functional.h"

#include "UICommon/WebUI/AVEncoder.h"

namespace WebUI
{

class AVStream;

class AVStreamPeer final
{
  friend AVStream;

public:
  explicit AVStreamPeer(std::shared_ptr<AVStream> av_stream);
  ~AVStreamPeer();

  bool HandleMessage(const std::string& message);

  // Callback for text messsage to send over the WebSocket.
  using SendCallbackType = Common::MoveOnlyFunction<void(const std::string&)>;
  void SetSendMessageCallback(SendCallbackType);

  // Callback for text messages received on the "control" data channel.
  using ControlCallbackType = Common::MoveOnlyFunction<void(const picojson::object&)>;
  void SetControlMessageCallback(ControlCallbackType);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

  std::shared_ptr<AVStream> m_av_stream;
};

class AVStream final
{
  friend AVStreamPeer;

public:
  AVStream();
  ~AVStream();

  AVStream(AVStream&&) = default;
  AVStream& operator=(AVStream&&) = default;

  AVStream(const AVStream&) = delete;
  AVStream& operator=(const AVStream&) = delete;

  void TakeAudioSamples(std::size_t frame_count,
                        Common::MoveOnlyFunction<void(std::span<u8>)> fill_buffer_callback);

  void PushVideoFrame(VideoEncoder::FrameDetails frame);

  void FlushVideo();

private:
  struct Impl;
  std::shared_ptr<Impl> m_impl;

  std::unique_ptr<AudioEncoder> m_audio_encoder;
  std::unique_ptr<VideoEncoder> m_video_encoder;
};

}  // namespace WebUI
