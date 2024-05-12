// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>
#include <thread>

#include "Common/Flag.h"
#include "Common/SocketContext.h"

namespace ExpansionInterface
{

class TAPServerConnection
{
public:
  using RecvCallback = std::function<void(std::string&&)>;

  TAPServerConnection(const std::string& destination, RecvCallback recv_cb,
                      std::size_t max_frame_size);

  bool Activate();
  void Deactivate();
  bool IsActivated();
  bool RecvInit();
  void RecvStart();
  void RecvStop();
  bool SendAndRemoveAllHDLCFrames(std::string* send_buf);
  bool SendFrame(const u8* frame, u32 size);

private:
  const std::string m_destination;
  const RecvCallback m_recv_cb;
  const std::size_t m_max_frame_size;
  Common::SocketContext m_socket_context;

  int m_fd = -1;
  std::thread m_read_thread;
  Common::Flag m_read_enabled;
  Common::Flag m_read_shutdown;

  bool StartReadThread();
  void ReadThreadHandler();
};

}  // namespace ExpansionInterface
