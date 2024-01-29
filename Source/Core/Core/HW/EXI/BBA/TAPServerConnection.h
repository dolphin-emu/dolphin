// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2ipdef.h>
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include <functional>
#include <thread>

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/SocketContext.h"
#include "Common/StringUtil.h"

namespace ExpansionInterface
{

class TAPServerConnection
{
public:
  TAPServerConnection(const std::string& destination, std::function<void(std::string&&)> recv_cb,
                      std::size_t max_frame_size);

  bool Activate();
  void Deactivate();
  bool IsActivated();
  bool RecvInit();
  void RecvStart();
  void RecvStop();
  bool SendAndRemoveAllHDLCFrames(std::string& send_buf);
  bool SendFrame(const u8* frame, u32 size);

private:
  enum class ReadState
  {
    Size,
    SizeHigh,
    Data,
    Skip,
  };

  std::string m_destination;
  std::function<void(std::string&&)> m_recv_cb;
  std::size_t m_max_frame_size;
  Common::SocketContext m_socket_context;

  int m_fd = -1;
  std::thread m_read_thread;
  Common::Flag m_read_enabled;
  Common::Flag m_read_shutdown;

  bool StartReadThread();
  void ReadThreadHandler();
};

}  // namespace ExpansionInterface
