// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/SocketContext.h"

#include "Common/Logging/Log.h"
#include "Common/Network.h"

namespace Common
{
#ifdef _WIN32
SocketContext::SocketContext()
{
  std::lock_guard<std::mutex> g(s_lock);
  if (s_num_objects == 0)
  {
    const int ret = WSAStartup(MAKEWORD(2, 2), &s_data);
    if (ret == 0)
    {
      INFO_LOG_FMT(COMMON, "WSAStartup succeeded, wVersion={}.{}, wHighVersion={}.{}",
                   int(LOBYTE(s_data.wVersion)), int(HIBYTE(s_data.wVersion)),
                   int(LOBYTE(s_data.wHighVersion)), int(HIBYTE(s_data.wHighVersion)));
    }
    else
    {
      // The WSAStartup function directly returns the extended error code in the return value.
      // A call to the WSAGetLastError function is not needed and should not be used.
      //
      // Source:
      // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsastartup
      ERROR_LOG_FMT(COMMON, "WSAStartup failed with error {}: {}", ret,
                    Common::DecodeNetworkError(ret));
    }
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
