// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <picojson.h>

#include "Common/Functional.h"

namespace WebUI
{

class AVStream;

class AVStreamRTCPeer final
{
public:
  explicit AVStreamRTCPeer(std::shared_ptr<AVStream> av_stream);
  ~AVStreamRTCPeer();

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
};

}  // namespace WebUI
