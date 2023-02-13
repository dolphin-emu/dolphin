// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceEthernet.h"

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

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/EXI/EXI_Device.h"

namespace ExpansionInterface
{

static int ConnectToDestination(const std::string& destination)
{
  if (destination.empty())
  {
    INFO_LOG_FMT(SP1, "Cannot connect: destination is empty\n");
    return -1;
  }

  size_t ss_size;
  struct sockaddr_storage ss;
  memset(&ss, 0, sizeof(ss));
  if (destination[0] != '/')
  {  // IP address or hostname
    size_t colon_offset = destination.find(':');
    if (colon_offset == std::string::npos)
    {
      INFO_LOG_FMT(SP1, "Destination IP address does not include port\n");
      return -1;
    }

    struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(&ss);
    sin->sin_addr.s_addr = htonl(sf::IpAddress(destination.substr(0, colon_offset)).toInteger());
    sin->sin_family = AF_INET;
    sin->sin_port = htons(stoul(destination.substr(colon_offset + 1)));
    ss_size = sizeof(*sin);
#ifndef _WIN32
  }
  else
  {  // UNIX socket
    struct sockaddr_un* sun = reinterpret_cast<struct sockaddr_un*>(&ss);
    if (destination.size() + 1 > sizeof(sun->sun_path))
    {
      INFO_LOG_FMT(SP1, "Socket path is too long, unable to init BBA\n");
      return -1;
    }
    sun->sun_family = AF_UNIX;
    strcpy(sun->sun_path, destination.c_str());
    ss_size = sizeof(*sun);
#else
  }
  else
  {
    INFO_LOG_FMT(SP1, "UNIX sockets are not supported on Windows\n");
    return -1;
#endif
  }

  int fd = socket(ss.ss_family, SOCK_STREAM, (ss.ss_family == AF_INET) ? IPPROTO_TCP : 0);
  if (fd == -1)
  {
    INFO_LOG_FMT(SP1, "Couldn't create socket; unable to init BBA\n");
    return -1;
  }

#ifdef __APPLE__
  int opt_no_sigpipe = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &opt_no_sigpipe, sizeof(opt_no_sigpipe)) < 0)
    INFO_LOG_FMT(SP1, "Failed to set SO_NOSIGPIPE on socket\n");
#endif

  if (connect(fd, reinterpret_cast<sockaddr*>(&ss), ss_size) == -1)
  {
    std::string s = Common::LastStrerrorString();
    INFO_LOG_FMT(SP1, "Couldn't connect socket ({}), unable to init BBA\n", s.c_str());
    close(fd);
    return -1;
  }

  return fd;
}

bool CEXIETHERNET::TAPServerNetworkInterface::Activate()
{
  if (IsActivated())
    return true;

  fd = ConnectToDestination(m_destination);

  INFO_LOG_FMT(SP1, "BBA initialized.");
  return RecvInit();
}

bool CEXIETHERNET::TAPServerNetworkInterface::SendFrame(const u8* frame, u32 size)
{
  {
    const std::string s = ArrayToString(frame, size, 0x10);
    INFO_LOG_FMT(SP1, "SendFrame {}\n{}", size, s);
  }

  auto size16 = u16(size);
  if (write(fd, &size16, 2) != 2)
  {
    ERROR_LOG_FMT(SP1, "SendFrame(): could not write size field");
    return false;
  }
  int written_bytes = write(fd, frame, size);
  if (u32(written_bytes) != size)
  {
    ERROR_LOG_FMT(SP1, "SendFrame(): expected to write {} bytes, instead wrote {}", size,
                  written_bytes);
    return false;
  }
  else
  {
    m_eth_ref->SendComplete();
    return true;
  }
}

void CEXIETHERNET::TAPServerNetworkInterface::ReadThreadHandler()
{
  while (!readThreadShutdown.IsSet())
  {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;
    if (select(fd + 1, &rfds, nullptr, nullptr, &timeout) <= 0)
      continue;

    u16 size;
    if (read(fd, &size, 2) != 2)
    {
      ERROR_LOG_FMT(SP1, "Failed to read size field from BBA: {}", Common::LastStrerrorString());
    }
    else
    {
      int read_bytes = read(fd, m_eth_ref->mRecvBuffer.get(), size);
      if (read_bytes < 0)
      {
        ERROR_LOG_FMT(SP1, "Failed to read packet data from BBA: {}", Common::LastStrerrorString());
      }
      else if (readEnabled.IsSet())
      {
        std::string data_string = ArrayToString(m_eth_ref->mRecvBuffer.get(), read_bytes, 0x10);
        INFO_LOG_FMT(SP1, "Read data: {}", data_string);
        m_eth_ref->mRecvBufferLength = read_bytes;
        m_eth_ref->RecvHandlePacket();
      }
    }
  }
}

bool CEXIETHERNET::TAPServerNetworkInterface::RecvInit()
{
  readThread = std::thread(&CEXIETHERNET::TAPServerNetworkInterface::ReadThreadHandler, this);
  return true;
}

}  // namespace ExpansionInterface
