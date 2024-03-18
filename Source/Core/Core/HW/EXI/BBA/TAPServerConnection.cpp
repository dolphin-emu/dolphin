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

#include <cstdlib>
#include <cstring>

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/EXI/EXI_Device.h"

namespace ExpansionInterface
{

#ifdef _WIN32
using ws_ssize_t = int;
#else
#define closesocket close
using ws_ssize_t = ssize_t;
#endif

#ifdef __linux__
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

TAPServerConnection::TAPServerConnection(const std::string& destination,
                                         std::function<void(std::string&&)> recv_cb,
                                         std::size_t max_frame_size)
    : m_destination(destination), m_recv_cb(recv_cb), m_max_frame_size(max_frame_size)
{
}

static int ConnectToDestination(const std::string& destination)
{
  if (destination.empty())
  {
    ERROR_LOG_FMT(SP1, "Cannot connect: destination is empty\n");
    return -1;
  }

  int ss_size;
  sockaddr_storage ss;
  std::memset(&ss, 0, sizeof(ss));
  if (destination[0] != '/')
  {
    // IP address or hostname
    const std::size_t colon_offset = destination.find(':');
    if (colon_offset == std::string::npos)
    {
      ERROR_LOG_FMT(SP1, "Destination IP address does not include port\n");
      return -1;
    }

    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(&ss);
    const sf::IpAddress dest_ip(destination.substr(0, colon_offset));
    if (dest_ip == sf::IpAddress::None || dest_ip == sf::IpAddress::Any)
    {
      ERROR_LOG_FMT(SP1, "Destination IP address is not valid\n");
      return -1;
    }
    sin->sin_addr.s_addr = htonl(dest_ip.toInteger());
    sin->sin_family = AF_INET;
    const std::string port_str = destination.substr(colon_offset + 1);
    const int dest_port = std::atoi(port_str.c_str());
    if (dest_port < 1 || dest_port > 65535)
    {
      ERROR_LOG_FMT(SP1, "Destination port is not valid\n");
      return -1;
    }
    sin->sin_port = htons(dest_port);
    ss_size = sizeof(*sin);
#ifndef _WIN32
  }
  else
  {
    // UNIX socket
    sockaddr_un* sun = reinterpret_cast<sockaddr_un*>(&ss);
    if (destination.size() + 1 > sizeof(sun->sun_path))
    {
      ERROR_LOG_FMT(SP1, "Socket path is too long; unable to create tapserver connection\n");
      return -1;
    }
    sun->sun_family = AF_UNIX;
    std::strcpy(sun->sun_path, destination.c_str());
    ss_size = sizeof(*sun);
#else
  }
  else
  {
    ERROR_LOG_FMT(SP1, "UNIX sockets are not supported on Windows\n");
    return -1;
#endif
  }

  const int fd = socket(ss.ss_family, SOCK_STREAM, (ss.ss_family == AF_INET) ? IPPROTO_TCP : 0);
  if (fd == -1)
  {
    ERROR_LOG_FMT(SP1, "Couldn't create socket; unable to create tapserver connection\n");
    return -1;
  }

#ifdef __APPLE__
  int opt_no_sigpipe = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &opt_no_sigpipe, sizeof(opt_no_sigpipe)) < 0)
    INFO_LOG_FMT(SP1, "Failed to set SO_NOSIGPIPE on socket\n");
#endif

  if (connect(fd, reinterpret_cast<sockaddr*>(&ss), ss_size) == -1)
  {
    INFO_LOG_FMT(SP1, "Couldn't connect socket ({}), unable to create tapserver connection\n",
                 Common::StrNetworkError());
    closesocket(fd);
    return -1;
  }

  return fd;
}

bool TAPServerConnection::Activate()
{
  if (IsActivated())
    return true;

  m_fd = ConnectToDestination(m_destination);
  if (m_fd < 0)
    return false;

  return RecvInit();
}

void TAPServerConnection::Deactivate()
{
  m_read_enabled.Clear();
  m_read_shutdown.Set();
  if (m_read_thread.joinable())
    m_read_thread.join();
  m_read_shutdown.Clear();

  if (m_fd >= 0)
    closesocket(m_fd);
  m_fd = -1;
}

bool TAPServerConnection::IsActivated()
{
  return (m_fd >= 0);
}

bool TAPServerConnection::RecvInit()
{
  m_read_thread = std::thread(&TAPServerConnection::ReadThreadHandler, this);
  return true;
}

void TAPServerConnection::RecvStart()
{
  m_read_enabled.Set();
}

void TAPServerConnection::RecvStop()
{
  m_read_enabled.Clear();
}

bool TAPServerConnection::SendAndRemoveAllHDLCFrames(std::string* send_buf)
{
  while (!send_buf->empty())
  {
    const std::size_t start_offset = send_buf->find(0x7E);
    if (start_offset == std::string::npos)
    {
      break;
    }
    const std::size_t end_sentinel_offset = send_buf->find(0x7E, start_offset + 1);
    if (end_sentinel_offset == std::string::npos)
    {
      break;
    }
    const std::size_t end_offset = end_sentinel_offset + 1;
    const std::size_t size = end_offset - start_offset;

    const u8 size_bytes[2] = {static_cast<u8>(size), static_cast<u8>(size >> 8)};
    if (send(m_fd, reinterpret_cast<const char*>(size_bytes), 2, SEND_FLAGS) != 2)
    {
      ERROR_LOG_FMT(SP1, "SendAndRemoveAllHDLCFrames(): could not write size field");
      return false;
    }
    const int written_bytes =
        send(m_fd, send_buf->data() + start_offset, static_cast<int>(size), SEND_FLAGS);
    if (u32(written_bytes) != size)
    {
      ERROR_LOG_FMT(SP1,
                    "SendAndRemoveAllHDLCFrames(): expected to write {} bytes, instead wrote {}",
                    size, written_bytes);
      return false;
    }
    *send_buf = send_buf->substr(end_offset);
  }
  return true;
}

bool TAPServerConnection::SendFrame(const u8* frame, u32 size)
{
  INFO_LOG_FMT(SP1, "SendFrame {}\n{}", size, ArrayToString(frame, size, 0x10));

  // On Windows, the data pointer is of type const char*; on other systems it is
  // of type const void*. This is the reason for the reinterpret_cast here and
  // in the other send/recv calls in this file.
  const u8 size_bytes[2] = {static_cast<u8>(size), static_cast<u8>(size >> 8)};
  if (send(m_fd, reinterpret_cast<const char*>(size_bytes), 2, SEND_FLAGS) != 2)
  {
    ERROR_LOG_FMT(SP1, "SendFrame(): could not write size field");
    return false;
  }
  const int written_bytes =
      send(m_fd, reinterpret_cast<const char*>(frame), static_cast<ws_ssize_t>(size), SEND_FLAGS);
  if (u32(written_bytes) != size)
  {
    ERROR_LOG_FMT(SP1, "SendFrame(): expected to write {} bytes, instead wrote {}", size,
                  written_bytes);
    return false;
  }
  return true;
}

void TAPServerConnection::ReadThreadHandler()
{
  enum class ReadState
  {
    SIZE,
    SIZE_HIGH,
    DATA,
    SKIP,
  };
  ReadState read_state = ReadState::SIZE;

  std::size_t frame_bytes_received = 0;
  std::size_t frame_bytes_expected = 0;
  std::string frame_data;

  while (!m_read_shutdown.IsSet())
  {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(m_fd, &rfds);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;
    int select_res = select(m_fd + 1, &rfds, nullptr, nullptr, &timeout);
    if (select_res < 0)
    {
      ERROR_LOG_FMT(SP1, "Can\'t poll tapserver fd: {}", Common::StrNetworkError());
      continue;
    }
    if (select_res == 0)
      continue;

    // The tapserver protocol is very simple: there is a 16-bit little-endian
    // size field, followed by that many bytes of packet data
    switch (read_state)
    {
    case ReadState::SIZE:
    {
      u8 size_bytes[2];
      const ws_ssize_t bytes_read = recv(m_fd, reinterpret_cast<char*>(size_bytes), 2, 0);
      if (bytes_read == 1)
      {
        read_state = ReadState::SIZE_HIGH;
        frame_bytes_expected = size_bytes[0];
      }
      else if (bytes_read == 2)
      {
        frame_bytes_expected = size_bytes[0] | (size_bytes[1] << 8);
        frame_data.resize(frame_bytes_expected, '\0');
        if (frame_bytes_expected > m_max_frame_size)
        {
          ERROR_LOG_FMT(SP1, "Packet is too large ({} bytes); dropping it", frame_bytes_expected);
          read_state = ReadState::SKIP;
        }
        else
        {
          // If read is disabled, we still need to actually read the frame in
          // order to avoid applying backpressure on the remote end, but we
          // should drop the frame instead of forwarding it to the client.
          read_state = m_read_enabled.IsSet() ? ReadState::DATA : ReadState::SKIP;
        }
      }
      else
      {
        ERROR_LOG_FMT(SP1, "Failed to read size field from destination: {}",
                      Common::StrNetworkError());
      }
      break;
    }
    case ReadState::SIZE_HIGH:
    {
      // This handles the annoying case where only one byte of the size field
      // was available earlier.
      u8 size_high = 0;
      const ws_ssize_t bytes_read = recv(m_fd, reinterpret_cast<char*>(&size_high), 1, 0);
      if (bytes_read != 1)
      {
        ERROR_LOG_FMT(SP1, "Failed to read split size field from destination: {}",
                      Common::StrNetworkError());
        break;
      }
      frame_bytes_expected |= (size_high << 8);
      frame_data.resize(frame_bytes_expected, '\0');
      if (frame_bytes_expected > m_max_frame_size)
      {
        ERROR_LOG_FMT(SP1, "Packet is too large ({} bytes); dropping it", frame_bytes_expected);
        read_state = ReadState::SKIP;
      }
      else
      {
        read_state = m_read_enabled.IsSet() ? ReadState::DATA : ReadState::SKIP;
      }
      break;
    }
    case ReadState::DATA:
    case ReadState::SKIP:
    {
      const std::size_t bytes_to_read = frame_data.size() - frame_bytes_received;
      const ws_ssize_t bytes_read = recv(m_fd, frame_data.data() + frame_bytes_received,
                                         static_cast<ws_ssize_t>(bytes_to_read), 0);
      if (bytes_read <= 0)
      {
        ERROR_LOG_FMT(SP1, "Failed to read data from destination: {}", Common::StrNetworkError());
        break;
      }
      frame_bytes_received += bytes_read;
      if (frame_bytes_received == frame_bytes_expected)
      {
        if (read_state == ReadState::DATA)
        {
          m_recv_cb(std::move(frame_data));
        }
        frame_data.clear();
        frame_bytes_received = 0;
        frame_bytes_expected = 0;
        read_state = ReadState::SIZE;
      }
      break;
    }
    }
  }
}

}  // namespace ExpansionInterface
