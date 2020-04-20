// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fcntl.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/EXI/EXI_DeviceEthernet.h"

namespace ExpansionInterface
{
bool CEXIETHERNET::UDPPhysicalNetworkInterface::Activate()
{
  if (IsActivated())
    return true;

  if ((m_read_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    ERROR_LOG(SP1, "Couldn't open udp read socket, unable to init BBA");
    return false;
  }

  if ((m_write_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    ERROR_LOG(SP1, "Couldn't open udp write socket, unable to init BBA");
    close(m_read_fd);
    return false;
  }

  struct sockaddr_in recvaddr
  {
  };
  struct sockaddr_in sendaddr
  {
  };

  recvaddr.sin_family = AF_INET;
  recvaddr.sin_addr.s_addr = INADDR_ANY;
  recvaddr.sin_port = htons(m_in_port);

  sendaddr.sin_family = AF_INET;
  sendaddr.sin_addr.s_addr = inet_addr(m_dest_ip.c_str());
  sendaddr.sin_port = htons(m_dest_port);

  if (bind(m_read_fd, reinterpret_cast<const struct sockaddr*>(&recvaddr), sizeof(recvaddr)) < 0)
  {
    ERROR_LOG(SP1, "Couldn't bind udp socket, unable to init BBA");
    close(m_read_fd);
    close(m_write_fd);
    return false;
  }

  if (connect(m_write_fd, reinterpret_cast<const struct sockaddr*>(&sendaddr), sizeof(sendaddr)) <
      0)
  {
    ERROR_LOG(SP1, "Couldn't connect udp socket, unable to init BBA");
    close(m_read_fd);
    close(m_write_fd);
    return false;
  }

  INFO_LOG(SP1, "BBA initialized.");

  return RecvInit();
}

void CEXIETHERNET::UDPPhysicalNetworkInterface::Deactivate()
{
  close(m_read_fd);
  close(m_write_fd);
  m_read_fd = -1;
  m_write_fd = -1;

  readEnabled.Clear();
  readThreadShutdown.Set();
  if (readThread.joinable())
    readThread.join();
}

bool CEXIETHERNET::UDPPhysicalNetworkInterface::IsActivated()
{
  return m_read_fd != -1 && m_write_fd == -1;
}

bool CEXIETHERNET::UDPPhysicalNetworkInterface::SendFrame(const u8* frame, u32 size)
{
  INFO_LOG(SP1, "SendFrame %x\n%s", size, ArrayToString(frame, size, 0x10).c_str());

  const int written_bytes = write(m_write_fd, frame, size);
  if (u32(written_bytes) != size)
  {
    ERROR_LOG(SP1, "SendFrame(): expected to write %d bytes, instead wrote %d, errno %d", size,
              written_bytes, errno);
    return false;
  }
  else
  {
    m_eth_ref->SendComplete();
    return true;
  }
}

void CEXIETHERNET::UDPPhysicalNetworkInterface::ReadThreadHandler(
    CEXIETHERNET::UDPPhysicalNetworkInterface* self)
{
  while (!self->readThreadShutdown.IsSet())
  {
    if (self->m_read_fd < 0)
      break;

    const int read_bytes = read(self->m_read_fd, self->m_eth_ref->mRecvBuffer.get(), BBA_RECV_SIZE);

    if (read_bytes < 0)
    {
      ERROR_LOG(SP1, "Failed to read from BBA, err=%d", read_bytes);
    }
    else if (self->readEnabled.IsSet())
    {
      INFO_LOG(SP1, "Read data: %s",
               ArrayToString(self->m_eth_ref->mRecvBuffer.get(), read_bytes, 0x10).c_str());
      self->m_eth_ref->mRecvBufferLength = read_bytes;
      self->m_eth_ref->RecvHandlePacket();
    }
  }
}

bool CEXIETHERNET::UDPPhysicalNetworkInterface::RecvInit()
{
  readThread = std::thread(ReadThreadHandler, this);
  return true;
}

void CEXIETHERNET::UDPPhysicalNetworkInterface::RecvStart()
{
  readEnabled.Set();
}

void CEXIETHERNET::UDPPhysicalNetworkInterface::RecvStop()
{
  readEnabled.Clear();
}
}  // namespace ExpansionInterface
