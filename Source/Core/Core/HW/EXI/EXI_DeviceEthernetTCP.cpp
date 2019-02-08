// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_DeviceEthernetTCP.h"

#include <memory>
#include <optional>
#include <string>

#include <SFML/Network.hpp>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/Memmap.h"

namespace ExpansionInterface
{
CEXIEthernetTCP::~CEXIEthernetTCP()
{
  DEBUG_LOG(SP1, "Ending receive thread...");
  m_read_enabled.Clear();
  m_read_thread_shutdown.Set();
  if (m_read_thread.joinable())
    m_read_thread.join();
}

bool CEXIEthernetTCP::Activate()
{
  if (IsActivated())
    return true;

  DEBUG_LOG(SP1, "Connecting...");

  m_socket = std::make_unique<sf::TcpSocket>();
  const sf::Socket::Status status = m_socket->connect(SConfig::GetInstance().m_bba_server, SConfig::GetInstance().m_bba_port);
  if (status != sf::Socket::Done)
  {
    ERROR_LOG(SP1, "Could not connect: %i", status);
    return false;
  }

  INFO_LOG(SP1, "Connected!");

  return RecvInit();
}

bool CEXIEthernetTCP::IsActivated() const
{
  return m_socket != nullptr;
}

bool CEXIEthernetTCP::SendFrame(const u8* frame, u32 size)
{
  INFO_LOG(SP1, "SendFrame %i", size);

  std::unique_ptr<u8[]> packet = std::make_unique<u8[]>(sizeof(int) + size);

  // prepend size
  for (std::size_t i = 0; i < sizeof(int); i++)
    packet[i] = (size >> (i * 8)) & 0xFF;

  memcpy(packet.get() + sizeof(int), frame, size);

  const sf::Socket::Status status = m_socket->send(packet.get(), sizeof(int) + size);

  if (status != sf::Socket::Done)
  {
    WARN_LOG(SP1, "Sending failed %i", status);
    return false;
  }

  SendComplete();
  return true;
}

bool CEXIEthernetTCP::RecvInit()
{
  m_read_thread = std::thread(ReadThreadHandler, this);
  return true;
}

void CEXIEthernetTCP::RecvStart()
{
  m_read_enabled.Set();
}

void CEXIEthernetTCP::RecvStop()
{
  m_read_enabled.Clear();
}

void CEXIEthernetTCP::ReadThreadHandler(CEXIEthernetTCP* self)
{
  sf::SocketSelector selector;
  selector.add(*self->m_socket);

  // are we currently waiting for size (4 bytes) or payload?
  enum { StateSize, StatePayload } state = StateSize;

  // buffer to store size temporarily
  u8 sizeBuffer[sizeof(int)];

  // how much of size or payload have we already received?
  std::size_t offset = 0;

  // payload size
  int size;

  while (!self->m_read_thread_shutdown.IsSet())
  {
    // blocks until socket has data to read or 100ms passed
    selector.wait(sf::milliseconds(100));

    std::size_t received;
    sf::Socket::Status status;

    switch (state)
    {
    case StateSize:
      // try to read remaining bytes for size
      status = self->m_socket->receive(&sizeBuffer + offset, sizeof(int) - offset, received);
      if (status != sf::Socket::Done)
      {
        ERROR_LOG(SP1, "Receiving failed %i", status);
        return;
      }

      DEBUG_LOG(SP1, "Received %lu bytes for size", received);

      // Have we got all bytes for size?
      offset += received;
      if(offset < sizeof(int))
        continue;
      offset = 0;

      // convert char array to size int
      size = 0;
      for(int i = 0; i < sizeof(int); i++)
        size |= sizeBuffer[i] << (i * 8);

      DEBUG_LOG(SP1, "Finished size %i", size);

      if (size > BBA_RECV_SIZE)
      {
        ERROR_LOG(SP1, "Received frame bigger than internal buffer!");
        return;
      }

      state = StatePayload;
    case StatePayload:
      // try to read remaining bytes for payload
      status = self->m_socket->receive(self->m_recv_buffer.get() + offset, size - offset, received);
      if (status != sf::Socket::Done)
      {
        ERROR_LOG(SP1, "Receiving failed %i", status);
        return;
      }

      DEBUG_LOG(SP1, "Receiving %lu bytes for payload", received);

      // Have we got all bytes for payload?
      offset += received;
      if(offset < size)
        continue;
      offset = 0;

      INFO_LOG(SP1, "Received payload %i", size);

      if (self->m_read_enabled.IsSet())
      {
        self->m_recv_buffer_length = size;
        self->RecvHandlePacket();
      }

      state = StateSize;
    }
  }

  DEBUG_LOG(SP1, "Receive thread ended!");
}
}  // namespace ExpansionInterface
