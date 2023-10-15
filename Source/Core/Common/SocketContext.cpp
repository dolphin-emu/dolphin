// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/SocketContext.h"

namespace Common
{
#ifdef _WIN32
SocketContext::SocketContext()
{
  std::lock_guard<std::mutex> g(s_lock);
  if (s_num_objects == 0)
  {
    static_cast<void>(WSAStartup(MAKEWORD(2, 2), &s_data));
  }
  s_num_objects++;
}
SocketContext::~SocketContext()
{
  std::lock_guard<std::mutex> g(s_lock);
  s_num_objects--;
  if (s_num_objects == 0)
  {
    WSACleanup();
  }
}

std::mutex SocketContext::s_lock;
size_t SocketContext::s_num_objects = 0;
WSADATA SocketContext::s_data;

#else
SocketContext::SocketContext() = default;
SocketContext::~SocketContext() = default;
#endif
}  // namespace Common
