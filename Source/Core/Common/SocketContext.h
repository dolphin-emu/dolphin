// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#endif

namespace Common
{
class SocketContext
{
public:
  SocketContext();
  ~SocketContext();

  SocketContext(const SocketContext&) = delete;
  SocketContext(SocketContext&&) = delete;

  SocketContext& operator=(const SocketContext&) = delete;
  SocketContext& operator=(SocketContext&&) = delete;

private:
#ifdef _WIN32
  WSADATA m_data;
#endif
};
}  // namespace Common
