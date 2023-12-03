// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceModem.h"

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

bool CEXIModem::TAPServerNetworkInterface::Activate()
{
  if (IsActivated())
    return true;

  m_fd = ConnectToDestination(m_destination);
  if (m_fd < 0)
  {
    return false;
  }

  INFO_LOG_FMT(SP1, "Modem initialized.");
  return RecvInit();
}

void CEXIModem::TAPServerNetworkInterface::Deactivate()
{
  if (m_fd >= 0)
  {
    pi_close(m_fd);
  }
  m_fd = -1;

  m_read_enabled.Clear();
  m_read_shutdown.Set();
  if (m_read_thread.joinable())
  {
    m_read_thread.join();
  }
  m_read_shutdown.Clear();
}

bool CEXIModem::TAPServerNetworkInterface::IsActivated()
{
  return (m_fd >= 0);
}

bool CEXIModem::TAPServerNetworkInterface::RecvInit()
{
  m_read_thread = std::thread(&CEXIModem::TAPServerNetworkInterface::ReadThreadHandler, this);
  return true;
}

void CEXIModem::TAPServerNetworkInterface::RecvStart()
{
  m_read_enabled.Set();
}

void CEXIModem::TAPServerNetworkInterface::RecvStop()
{
  m_read_enabled.Clear();
}

bool CEXIModem::TAPServerNetworkInterface::SendFrames()
{
  while (!m_modem_ref->m_send_buffer.empty())
  {
    size_t start_offset = m_modem_ref->m_send_buffer.find(0x7E);
    if (start_offset == std::string::npos)
    {
      break;
    }
    size_t end_sentinel_offset = m_modem_ref->m_send_buffer.find(0x7E, start_offset + 1);
    if (end_sentinel_offset == std::string::npos)
    {
      break;
    }
    size_t end_offset = end_sentinel_offset + 1;
    size_t size = end_offset - start_offset;

    uint8_t size_bytes[2] = {static_cast<u8>(size), static_cast<u8>(size >> 8)};
    if (send(m_fd, size_bytes, 2, SEND_FLAGS) != 2)
    {
      ERROR_LOG_FMT(SP1, "SendFrames(): could not write size field");
      return false;
    }
    int written_bytes =
        send(m_fd, m_modem_ref->m_send_buffer.data() + start_offset, size, SEND_FLAGS);
    if (u32(written_bytes) != size)
    {
      ERROR_LOG_FMT(SP1, "SendFrames(): expected to write {} bytes, instead wrote {}", size,
                    written_bytes);
      return false;
    }
    else
    {
      m_modem_ref->m_send_buffer = m_modem_ref->m_send_buffer.substr(end_offset);
      m_modem_ref->SendComplete();
    }
  }
  return true;
}

void CEXIModem::TAPServerNetworkInterface::ReadThreadHandler()
{
  enum class ReadState
  {
    SIZE,
    SIZE_HIGH,
    DATA,
    SKIP,
  };
  ReadState read_state = ReadState::SIZE;

  size_t frame_bytes_received = 0;
  size_t frame_bytes_expected = 0;
  std::string frame_data;

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
    switch (read_state)
    {
    case ReadState::SIZE:
    {
      u8 size_bytes[2];
      ws_ssize_t bytes_read = recv(m_fd, reinterpret_cast<char*>(size_bytes), 2, 0);
      if (bytes_read == 1)
      {
        read_state = ReadState::SIZE_HIGH;
        frame_bytes_expected = size_bytes[0];
      }
      else if (bytes_read == 2)
      {
        frame_bytes_expected = size_bytes[0] | (size_bytes[1] << 8);
        frame_data.resize(frame_bytes_expected, '\0');
        if (frame_bytes_expected > MODEM_RECV_SIZE)
        {
          ERROR_LOG_FMT(SP1, "Packet is too large ({} bytes); dropping it", frame_bytes_expected);
          read_state = ReadState::SKIP;
        }
        else
        {
          read_state = ReadState::DATA;
        }
      }
      else
      {
        ERROR_LOG_FMT(SP1, "Failed to read size field from destination: {}",
                      Common::LastStrerrorString());
      }
      break;
    }
    case ReadState::SIZE_HIGH:
    {
      // This handles the annoying case where only one byte of the size field
      // was available earlier.
      u8 size_high = 0;
      ws_ssize_t bytes_read = recv(m_fd, reinterpret_cast<char*>(&size_high), 1, 0);
      if (bytes_read == 1)
      {
        frame_bytes_expected |= (size_high << 8);
        frame_data.resize(frame_bytes_expected, '\0');
        if (frame_bytes_expected > MODEM_RECV_SIZE)
        {
          ERROR_LOG_FMT(SP1, "Packet is too large ({} bytes); dropping it", frame_bytes_expected);
          read_state = ReadState::SKIP;
        }
        else
        {
          read_state = ReadState::DATA;
        }
      }
      else
      {
        ERROR_LOG_FMT(SP1, "Failed to read split size field from destination: {}",
                      Common::LastStrerrorString());
      }
      break;
    }
    case ReadState::DATA:
    case ReadState::SKIP:
    {
      ws_ssize_t bytes_read = recv(m_fd, frame_data.data() + frame_bytes_received,
                                   frame_data.size() - frame_bytes_received, 0);
      if (bytes_read <= 0)
      {
        ERROR_LOG_FMT(SP1, "Failed to read data from destination: {}",
                      Common::LastStrerrorString());
      }
      else
      {
        frame_bytes_received += bytes_read;
        if (frame_bytes_received == frame_bytes_expected)
        {
          if (read_state == ReadState::DATA)
          {
            m_modem_ref->AddToReceiveBuffer(std::move(frame_data));
          }
          frame_data.clear();
          frame_bytes_received = 0;
          frame_bytes_expected = 0;
          read_state = ReadState::SIZE;
        }
      }
      break;
    }
    }
  }
}

}  // namespace ExpansionInterface
