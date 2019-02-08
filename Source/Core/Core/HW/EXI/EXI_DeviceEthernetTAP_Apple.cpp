// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fcntl.h>
#include <unistd.h>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/EXI/EXI_DeviceEthernetTAP.h"

namespace ExpansionInterface
{
CEXIEthernetTAP::~CEXIEthernetTAP()
{
  close(fd);
  fd = -1;

  m_read_enabled.Clear();
  m_read_thread_shutdown.Set();
  if (m_read_thread.joinable())
    m_read_thread.join();
}

bool CEXIEthernetTAP::Activate()
{
  if (IsActivated())
    return true;

  // Assumes TunTap OS X is installed, and /dev/tun0 is not in use
  // and readable / writable by the logged-in user

  if ((fd = open("/dev/tap0", O_RDWR)) < 0)
  {
    ERROR_LOG(SP1, "Couldn't open /dev/tap0, unable to init BBA");
    return false;
  }

  INFO_LOG(SP1, "BBA initialized.");
  return RecvInit();
}

bool CEXIEthernetTAP::IsActivated() const
{
  return fd != -1;
}

bool CEXIEthernetTAP::SendFrame(const u8* frame, u32 size)
{
  INFO_LOG(SP1, "SendFrame %x\n%s", size, ArrayToString(frame, size, 0x10).c_str());

  int writtenBytes = write(fd, frame, size);
  if ((u32)writtenBytes != size)
  {
    ERROR_LOG(SP1, "SendFrame(): expected to write %d bytes, instead wrote %d", size, writtenBytes);
    return false;
  }
  else
  {
    SendComplete();
    return true;
  }
}

void CEXIEthernetTAP::ReadThreadHandler(CEXIEthernetTAP* self)
{
  while (!self->m_read_thread_shutdown.IsSet())
  {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(self->fd, &rfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;
    if (select(self->fd + 1, &rfds, nullptr, nullptr, &timeout) <= 0)
      continue;

    int readBytes = read(self->fd, self->m_recv_buffer.get(), BBA_RECV_SIZE);
    if (readBytes < 0)
    {
      ERROR_LOG(SP1, "Failed to read from BBA, err=%d", readBytes);
    }
    else if (self->m_read_enabled.IsSet())
    {
      INFO_LOG(SP1, "Read data: %s",
               ArrayToString(self->m_recv_buffer.get(), readBytes, 0x10).c_str());
      self->m_recv_buffer_length = readBytes;
      self->RecvHandlePacket();
    }
  }
}

bool CEXIEthernetTAP::RecvInit()
{
  m_read_thread = std::thread(ReadThreadHandler, this);
  return true;
}

void CEXIEthernetTAP::RecvStart()
{
  m_read_enabled.Set();
}

void CEXIEthernetTAP::RecvStop()
{
  m_read_enabled.Clear();
}
}  // namespace ExpansionInterface
