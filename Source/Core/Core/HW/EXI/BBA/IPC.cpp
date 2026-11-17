// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CPPIPC
#include <cstring>
#include <thread>

#include <libipc/ipc.h>

#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#endif

#include "Core/HW/EXI/EXI_DeviceEthernet.h"

namespace ExpansionInterface
{
#ifdef HAVE_CPPIPC
class IPCBBAInterface : public CEXIETHERNET::NetworkInterface
{
public:
  explicit IPCBBAInterface(CEXIETHERNET* const eth_ref) : NetworkInterface(eth_ref) {}

  bool Activate() override;
  void Deactivate() override;
  bool IsActivated() override;
  bool SendFrame(const u8* frame, u32 size) override;
  bool RecvInit() override;
  void RecvStart() override;
  void RecvStop() override;

private:
  void ReadThreadHandler();

  bool m_active{};
  ipc::channel m_channel;
  std::thread m_read_thread;
  Common::Flag m_read_enabled;
  Common::Flag m_read_thread_shutdown;
};

std::unique_ptr<CEXIETHERNET::NetworkInterface> CreateIPCBBAInterface(CEXIETHERNET* eth_ref)
{
  return std::make_unique<IPCBBAInterface>(eth_ref);
}

bool IPCBBAInterface::Activate()
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

void IPCBBAInterface::Deactivate()
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

bool IPCBBAInterface::IsActivated()
{
  return m_active;
}

bool IPCBBAInterface::SendFrame(const u8* const frame, const u32 size)
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

bool IPCBBAInterface::RecvInit()
{
  m_read_thread = std::thread(&IPCBBAInterface::ReadThreadHandler, this);
  return true;
}

void IPCBBAInterface::RecvStart()
{
  m_read_enabled.Set();
}

void IPCBBAInterface::RecvStop()
{
  m_read_enabled.Clear();
}

void IPCBBAInterface::ReadThreadHandler()
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
#else
std::unique_ptr<CEXIETHERNET::NetworkInterface> CreateIPCBBAInterface(CEXIETHERNET* eth_ref)
{
  return std::make_unique<CEXIETHERNET::NetworkInterface>(eth_ref);
}
#endif
}  // namespace ExpansionInterface
