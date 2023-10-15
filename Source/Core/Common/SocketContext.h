// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#include <mutex>
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
  static std::mutex s_lock;
  static size_t s_num_objects;
  static WSADATA s_data;
#endif
};
}  // namespace Common
