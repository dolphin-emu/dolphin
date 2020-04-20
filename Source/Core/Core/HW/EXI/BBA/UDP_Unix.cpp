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

  if ((readFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    ERROR_LOG(SP1, "Couldn't open udp read socket, unable to init BBA");
    return false;
  }

  if ((writeFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    ERROR_LOG(SP1, "Couldn't open udp write socket, unable to init BBA");
    close(readFD);
    return false;
  }

  struct sockaddr_in recvaddr, sendaddr;
  memset(&recvaddr, 0, sizeof(recvaddr));
  memset(&sendaddr, 0, sizeof(sendaddr));

  recvaddr.sin_family = AF_INET;
  recvaddr.sin_addr.s_addr = INADDR_ANY;
  recvaddr.sin_port = htons(inPort);

  sendaddr.sin_family = AF_INET;
  sendaddr.sin_addr.s_addr = inet_addr(destIp.c_str());
  sendaddr.sin_port = htons(destPort);

  if (bind(readFD, (const struct sockaddr*)&recvaddr, sizeof(recvaddr)) < 0)
  {
    ERROR_LOG(SP1, "Couldn't bind udp socket, unable to init BBA");
    close(readFD);
    close(writeFD);
    return false;
  }

  if (connect(writeFD, (const struct sockaddr*)&sendaddr, sizeof(sendaddr)) < 0)
  {
    ERROR_LOG(SP1, "Couldn't connect udp socket, unable to init BBA");
    close(readFD);
    close(writeFD);
    return false;
  }

  INFO_LOG(SP1, "BBA initialized.");

  return RecvInit();
}

void CEXIETHERNET::UDPPhysicalNetworkInterface::Deactivate()
{
  close(readFD);
  close(writeFD);
  readFD = -1;
  writeFD = -1;

  readEnabled.Clear();
  readThreadShutdown.Set();
  if (readThread.joinable())
    readThread.join();
}

bool CEXIETHERNET::UDPPhysicalNetworkInterface::IsActivated()
{
  return readFD != -1 && writeFD == -1;
}

bool CEXIETHERNET::UDPPhysicalNetworkInterface::SendFrame(const u8* frame, u32 size)
{
  INFO_LOG(SP1, "SendFrame %x\n%s", size, ArrayToString(frame, size, 0x10).c_str());

  int writtenBytes = write(writeFD, frame, size);
  if ((u32)writtenBytes != size)
  {
    ERROR_LOG(SP1, "SendFrame(): expected to write %d bytes, instead wrote %d, errno %d", size,
              writtenBytes, errno);
    return false;
  }
  else
  {
    ethRef->SendComplete();
    return true;
  }
}

void CEXIETHERNET::UDPPhysicalNetworkInterface::ReadThreadHandler(
    CEXIETHERNET::UDPPhysicalNetworkInterface* self)
{
  while (!self->readThreadShutdown.IsSet())
  {
    if (self->readFD < 0)
      break;

    int readBytes = read(self->readFD, self->ethRef->mRecvBuffer.get(), BBA_RECV_SIZE);

    if (readBytes < 0)
    {
      ERROR_LOG(SP1, "Failed to read from BBA, err=%d", readBytes);
    }
    else if (self->readEnabled.IsSet())
    {
      INFO_LOG(SP1, "Read data: %s",
               ArrayToString(self->ethRef->mRecvBuffer.get(), readBytes, 0x10).c_str());
      self->ethRef->mRecvBufferLength = readBytes;
      self->ethRef->RecvHandlePacket();
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
