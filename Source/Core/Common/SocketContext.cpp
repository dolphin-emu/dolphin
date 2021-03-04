// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/SocketContext.h"

namespace Common
{
#ifdef _WIN32
SocketContext::SocketContext()
{
  static_cast<void>(WSAStartup(MAKEWORD(2, 2), &m_data));
}
SocketContext::~SocketContext()
{
  WSACleanup();
}
#else
SocketContext::SocketContext() = default;
SocketContext::~SocketContext() = default;
#endif
}  // namespace Common
