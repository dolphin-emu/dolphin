// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include <libipc/ipc.h>

#include "Common/Logging/Log.h"
#include "Core/HW/EXI/EXI_DeviceEthernet.h"

namespace ExpansionInterface
{

bool CEXIETHERNET::IPCBBAInterface::Activate()
{
  if (m_active)
    return false;

  m_channel.connect("dolphin-emu-bba-ipc", ipc::sender | ipc::receiver);
  if (!m_channel.valid())
    return false;

  m_read_enabled.Clear();
  m_read_thread_shutdown.Clear();

  m_active = true;

  return RecvInit();
}

void CEXIETHERNET::IPCBBAInterface::Deactivate()
{
  m_read_enabled.Clear();
  m_read_thread_shutdown.Set();

  if (m_read_thread.joinable())
  {
    m_read_thread.join();
  }

  m_channel.disconnect();

  m_active = false;
}

bool CEXIETHERNET::IPCBBAInterface::IsActivated()
{
  return m_active;
}

bool CEXIETHERNET::IPCBBAInterface::SendFrame(const u8* const frame, const u32 size)
{
  if (!m_active)
    return false;

  static constexpr u64 TIMEOUT_IN_MS{3000};
  if (!m_channel.send(frame, size, TIMEOUT_IN_MS))
  {
    ERROR_LOG_FMT(SP1, "Failed to send frame");
    return false;
  }

  m_eth_ref->SendComplete();

  return true;
}

bool CEXIETHERNET::IPCBBAInterface::RecvInit()
{
  m_read_thread = std::thread(&CEXIETHERNET::IPCBBAInterface::ReadThreadHandler, this);
  return true;
}

void CEXIETHERNET::IPCBBAInterface::RecvStart()
{
  m_read_enabled.Set();
}

void CEXIETHERNET::IPCBBAInterface::RecvStop()
{
  m_read_enabled.Clear();
}

void CEXIETHERNET::IPCBBAInterface::ReadThreadHandler()
{
  while (!m_read_thread_shutdown.IsSet())
  {
    const ipc::buff_t buffer{m_channel.recv(50)};
    if (buffer.empty() || !m_read_enabled.IsSet())
      continue;

    const u8* const frame{reinterpret_cast<const u8*>(buffer.data())};
    const u64 size{buffer.size()};

    std::memcpy(m_eth_ref->mRecvBuffer.get(), frame, size);
    m_eth_ref->mRecvBufferLength = static_cast<u32>(size);
    m_eth_ref->RecvHandlePacket();
  }
}

}  // namespace ExpansionInterface
