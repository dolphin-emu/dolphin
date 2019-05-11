// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/EXI/EXI_DeviceEthernetTAP.h"

#ifdef __linux__
#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif

namespace ExpansionInterface
{
#define NOTIMPLEMENTED NOTICE_LOG(SP1, "%s not implemented for your UNIX", __PRETTY_FUNCTION__);

CEXIEthernetTAP::~CEXIEthernetTAP()
{
#ifdef __linux__
  close(m_fd);

  m_read_enabled.Clear();
  m_read_thread_shutdown.Set();
  if (m_read_thread.joinable())
    m_read_thread.join();
#else
  NOTIMPLEMENTED;
#endif
}

bool CEXIEthernetTAP::Activate()
{
#ifdef __linux__
  if (IsActivated())
    return true;

  // Assumes that there is a TAP device named "Dolphin" preconfigured for
  // bridge/NAT/whatever the user wants it configured.

  if ((m_fd = open("/dev/net/tun", O_RDWR)) < 0)
  {
    ERROR_LOG(SP1, "Couldn't open /dev/net/tun, unable to init BBA");
    return false;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_ONE_QUEUE;

  const int MAX_INTERFACES = 32;
  for (int i = 0; i < MAX_INTERFACES; ++i)
  {
    strncpy(ifr.ifr_name, StringFromFormat("Dolphin%d", i).c_str(), IFNAMSIZ);

    int err;
    if ((err = ioctl(m_fd, TUNSETIFF, (void*)&ifr)) < 0)
    {
      if (i == (MAX_INTERFACES - 1))
      {
        close(m_fd);
        m_fd = -1;
        ERROR_LOG(SP1, "TUNSETIFF failed: Interface=%s err=%d", ifr.ifr_name, err);
        return false;
      }
    }
    else
    {
      break;
    }
  }
  ioctl(m_fd, TUNSETNOCSUM, 1);

  INFO_LOG(SP1, "BBA initialized with associated tap %s", ifr.ifr_name);
  return RecvInit();
#else
  NOTIMPLEMENTED;
  return false;
#endif
}

bool CEXIEthernetTAP::IsActivated() const
{
#ifdef __linux__
  return m_fd != -1;
#else
  return false;
#endif
}

bool CEXIEthernetTAP::SendFrame(const u8* frame, u32 size)
{
#ifdef __linux__
  int writtenBytes = write(m_fd, frame, size);
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
#else
  NOTIMPLEMENTED;
  return false;
#endif
}

bool CEXIEthernetTAP::RecvInit()
{
#ifdef __linux__
  m_read_thread = std::thread(ReadThreadHandler, this);
  return true;
#else
  NOTIMPLEMENTED;
  return false;
#endif
}

void CEXIEthernetTAP::RecvStart()
{
#ifdef __linux__
  m_read_enabled.Set();
#else
  NOTIMPLEMENTED;
#endif
}

void CEXIEthernetTAP::RecvStop()
{
#ifdef __linux__
  m_read_enabled.Clear();
#else
  NOTIMPLEMENTED("RecvStop");
#endif
}

#ifdef __linux__
void CEXIEthernetTAP::ReadThreadHandler(CEXIEthernetTAP* self)
{
  while (!self->m_read_thread_shutdown.IsSet())
  {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(self->m_fd, &rfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;
    if (select(self->m_fd + 1, &rfds, nullptr, nullptr, &timeout) <= 0)
      continue;

    int readBytes = read(self->m_fd, &self->m_recv_buffer, BBA_RECV_SIZE);
    if (readBytes < 0)
    {
      ERROR_LOG(SP1, "Failed to read from BBA, err=%d", readBytes);
    }
    else if (self->m_read_enabled.IsSet())
    {
      self->m_recv_buffer_length = readBytes;
      self->RecvHandlePacket();
    }
  }
}
#endif
}  // namespace ExpansionInterface
