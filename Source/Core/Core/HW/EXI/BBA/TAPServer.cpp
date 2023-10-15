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

#ifdef _WIN32
static constexpr auto pi_close = &closesocket;
using ws_ssize_t = int;
#else
static constexpr auto pi_close = &close;
using ws_ssize_t = ssize_t;
#endif

#ifdef __LINUX__
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

static int ConnectToDestination(const std::string& destination)
{
  if (destination.empty())
  {
    ERROR_LOG_FMT(SP1, "Cannot connect: destination is empty\n");
    return -1;
  }

  int ss_size;
  struct sockaddr_storage ss;
  memset(&ss, 0, sizeof(ss));
  if (destination[0] != '/')
  {
    // IP address or hostname
    size_t colon_offset = destination.find(':');
    if (colon_offset == std::string::npos)
    {
      ERROR_LOG_FMT(SP1, "Destination IP address does not include port\n");
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
  {
    // UNIX socket
    struct sockaddr_un* sun = reinterpret_cast<struct sockaddr_un*>(&ss);
    if (destination.size() + 1 > sizeof(sun->sun_path))
    {
      ERROR_LOG_FMT(SP1, "Socket path is too long, unable to init BBA\n");
      return -1;
    }
    sun->sun_family = AF_UNIX;
    strcpy(sun->sun_path, destination.c_str());
    ss_size = sizeof(*sun);
#else
  }
  else
  {
    ERROR_LOG_FMT(SP1, "UNIX sockets are not supported on Windows\n");
    return -1;
#endif
  }

  int fd = socket(ss.ss_family, SOCK_STREAM, (ss.ss_family == AF_INET) ? IPPROTO_TCP : 0);
  if (fd == -1)
  {
    ERROR_LOG_FMT(SP1, "Couldn't create socket; unable to init BBA\n");
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
    pi_close(fd);
    return -1;
  }

  return fd;
}

bool CEXIETHERNET::TAPServerNetworkInterface::Activate()
{
  if (IsActivated())
    return true;

  m_fd = ConnectToDestination(m_destination);

  INFO_LOG_FMT(SP1, "BBA initialized.");
  return RecvInit();
}

void CEXIETHERNET::TAPServerNetworkInterface::Deactivate()
{
  pi_close(m_fd);
  m_fd = -1;

  m_read_enabled.Clear();
  m_read_shutdown.Set();
  if (m_read_thread.joinable())
    m_read_thread.join();
}

bool CEXIETHERNET::TAPServerNetworkInterface::IsActivated()
{
  return (m_fd >= 0);
}

bool CEXIETHERNET::TAPServerNetworkInterface::RecvInit()
{
  m_read_thread = std::thread(&CEXIETHERNET::TAPServerNetworkInterface::ReadThreadHandler, this);
  return true;
}

void CEXIETHERNET::TAPServerNetworkInterface::RecvStart()
{
  m_read_enabled.Set();
}

void CEXIETHERNET::TAPServerNetworkInterface::RecvStop()
{
  m_read_enabled.Clear();
}

bool CEXIETHERNET::TAPServerNetworkInterface::SendFrame(const u8* frame, u32 size)
{
  {
    const std::string s = ArrayToString(frame, size, 0x10);
    INFO_LOG_FMT(SP1, "SendFrame {}\n{}", size, s);
  }

  // On Windows, the data pointer is of type const char*; on other systems it is
  // of type const void*. This is the reason for the reinterpret_cast here and
  // in the other send/recv calls in this file.
  u8 size_bytes[2] = {static_cast<u8>(size), static_cast<u8>(size >> 8)};
  if (send(m_fd, reinterpret_cast<const char*>(size_bytes), 2, SEND_FLAGS) != 2)
  {
    ERROR_LOG_FMT(SP1, "SendFrame(): could not write size field");
    return false;
  }
  int written_bytes = send(m_fd, reinterpret_cast<const char*>(frame), size, SEND_FLAGS);
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
  while (!m_read_shutdown.IsSet())
  {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(m_fd, &rfds);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;
    if (select(m_fd + 1, &rfds, nullptr, nullptr, &timeout) <= 0)
      continue;

    // The tapserver protocol is very simple: there is a 16-bit little-endian
    // size field, followed by that many bytes of packet data
    switch (m_read_state)
    {
    case ReadState::Size:
    {
      u8 size_bytes[2];
      ws_ssize_t bytes_read = recv(m_fd, reinterpret_cast<char*>(size_bytes), 2, 0);
      if (bytes_read == 1)
      {
        m_read_state = ReadState::SizeHigh;
        m_read_packet_bytes_remaining = size_bytes[0];
      }
      else if (bytes_read == 2)
      {
        m_read_packet_bytes_remaining = size_bytes[0] | (size_bytes[1] << 8);
        if (m_read_packet_bytes_remaining > BBA_RECV_SIZE)
        {
          ERROR_LOG_FMT(SP1, "Packet is too large ({} bytes); dropping it",
                        m_read_packet_bytes_remaining);
          m_read_state = ReadState::Skip;
        }
        else
        {
          m_read_state = ReadState::Data;
        }
      }
      else
      {
        ERROR_LOG_FMT(SP1, "Failed to read size field from BBA: {}", Common::LastStrerrorString());
      }
      break;
    }
    case ReadState::SizeHigh:
    {
      // This handles the annoying case where only one byte of the size field
      // was available earlier.
      u8 size_high = 0;
      ws_ssize_t bytes_read = recv(m_fd, reinterpret_cast<char*>(&size_high), 1, 0);
      if (bytes_read == 1)
      {
        m_read_packet_bytes_remaining |= (size_high << 8);
        if (m_read_packet_bytes_remaining > BBA_RECV_SIZE)
        {
          ERROR_LOG_FMT(SP1, "Packet is too large ({} bytes); dropping it",
                        m_read_packet_bytes_remaining);
          m_read_state = ReadState::Skip;
        }
        else
        {
          m_read_state = ReadState::Data;
        }
      }
      else
      {
        ERROR_LOG_FMT(SP1, "Failed to read split size field from BBA: {}",
                      Common::LastStrerrorString());
      }
      break;
    }
    case ReadState::Data:
    {
      ws_ssize_t bytes_read =
          recv(m_fd, reinterpret_cast<char*>(m_eth_ref->mRecvBuffer.get() + m_read_packet_offset),
               m_read_packet_bytes_remaining, 0);
      if (bytes_read <= 0)
      {
        ERROR_LOG_FMT(SP1, "Failed to read data from BBA: {}", Common::LastStrerrorString());
      }
      else
      {
        m_read_packet_offset += bytes_read;
        m_read_packet_bytes_remaining -= bytes_read;
        if (m_read_packet_bytes_remaining == 0)
        {
          m_eth_ref->mRecvBufferLength = m_read_packet_offset;
          m_eth_ref->RecvHandlePacket();
          m_read_state = ReadState::Size;
          m_read_packet_offset = 0;
        }
      }
      break;
    }
    case ReadState::Skip:
    {
      ws_ssize_t bytes_read = recv(m_fd, reinterpret_cast<char*>(m_eth_ref->mRecvBuffer.get()),
                                   std::min<int>(m_read_packet_bytes_remaining, BBA_RECV_SIZE), 0);
      if (bytes_read <= 0)
      {
        ERROR_LOG_FMT(SP1, "Failed to read data from BBA: {}", Common::LastStrerrorString());
      }
      else
      {
        m_read_packet_bytes_remaining -= bytes_read;
        if (m_read_packet_bytes_remaining == 0)
        {
          m_read_state = ReadState::Size;
          m_read_packet_offset = 0;
        }
      }
      break;
    }
    }
  }
}

}  // namespace ExpansionInterface
