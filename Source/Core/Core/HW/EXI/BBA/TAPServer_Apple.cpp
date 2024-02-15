// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceEthernet.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/EXI/EXI_Device.h"

namespace ExpansionInterface
{
// This interface is only implemented on macOS, since macOS needs a replacement
// for TunTap when the kernel extension is no longer supported. This interface
// only appears in the menu on macOS, so on other platforms, it does nothing and
// refuses to activate.

constexpr char socket_path[] = "/tmp/dolphin-tap";

bool CEXIETHERNET::TAPServerNetworkInterface::Activate()
{
  if (IsActivated())
    return true;

  sockaddr_un sun = {};
  if (sizeof(socket_path) > sizeof(sun.sun_path))
  {
    ERROR_LOG_FMT(SP1, "Socket path is too long, unable to init BBA");
    return false;
  }
  sun.sun_family = AF_UNIX;
  strcpy(sun.sun_path, socket_path);

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1)
  {
    ERROR_LOG_FMT(SP1, "Couldn't create socket, unable to init BBA");
    return false;
  }

  if (connect(fd, reinterpret_cast<sockaddr*>(&sun), sizeof(sun)) == -1)
  {
    ERROR_LOG_FMT(SP1, "Couldn't connect socket ({}), unable to init BBA",
                  Common::LastStrerrorString());
    close(fd);
    fd = -1;
    return false;
  }

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
