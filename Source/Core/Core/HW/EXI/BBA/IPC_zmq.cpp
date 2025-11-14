// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <bit>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include <FileLockFactory.hpp>
#include <zmq.h>

#include "Core/HW/EXI/EXI_DeviceEthernet.h"

namespace ExpansionInterface
{

CEXIETHERNET::IPCBBAInterface::IPCBBAInterface(CEXIETHERNET* const eth_ref)
    : CEXIETHERNET::NetworkInterface(eth_ref), m_context(zmq_ctx_new()),
      m_proxy_thread(&CEXIETHERNET::IPCBBAInterface::ProxyThreadHandler, this)
{
}

CEXIETHERNET::IPCBBAInterface::~IPCBBAInterface()
{
  m_proxy_thread_shutdown.Set();

  {
    std::lock_guard<std::mutex> lock{m_proxy_mutex};

    if (m_proxy_publisher)
    {
      zmq_close(m_proxy_publisher);
      m_proxy_publisher = nullptr;
    }
    if (m_proxy_subscriber)
    {
      zmq_close(m_proxy_subscriber);
      m_proxy_subscriber = nullptr;
    }
  }

  zmq_ctx_term(m_context);
  m_context = nullptr;

  if (m_proxy_thread.joinable())
  {
    m_proxy_thread.join();
  }
}

bool CEXIETHERNET::IPCBBAInterface::Activate()
{
  if (m_active)
    return false;

  const u32 linger{0};

  m_publisher = zmq_socket(m_context, ZMQ_PUB);
  zmq_setsockopt(m_publisher, ZMQ_LINGER, &linger, sizeof(linger));
  zmq_connect(m_publisher, "ipc:///tmp/dolphin-bba-outbox");

  m_subscriber = zmq_socket(m_context, ZMQ_SUB);
  zmq_setsockopt(m_subscriber, ZMQ_LINGER, &linger, sizeof(linger));
  zmq_setsockopt(m_subscriber, ZMQ_SUBSCRIBE, "", 0);
  zmq_connect(m_subscriber, "ipc:///tmp/dolphin-bba-inbox");

  m_read_enabled.Clear();
  m_read_thread_shutdown.Clear();

  m_active = true;

  return RecvInit();
}

void CEXIETHERNET::IPCBBAInterface::Deactivate()
{
  if (!m_active)
    return;

  m_read_enabled.Clear();
  m_read_thread_shutdown.Set();

  if (m_read_thread.joinable())
  {
    m_read_thread.join();
  }

  zmq_close(m_publisher);
  zmq_close(m_subscriber);
  m_publisher = nullptr;
  m_subscriber = nullptr;

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

  std::vector<u8> message;
  const u64 self{std::bit_cast<u64>(this)};
  message.resize(sizeof(self) + static_cast<u64>(size));
  std::memcpy(message.data(), &self, sizeof(self));
  std::memcpy(message.data() + sizeof(self), frame, static_cast<u64>(size));

  zmq_send(m_publisher, message.data(), message.size(), 0);
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
  const u64 self{std::bit_cast<u64>(this)};

  std::vector<u8> buffer;
  buffer.resize(sizeof(self) + BBA_RECV_SIZE);

  while (!m_read_thread_shutdown.IsSet())
  {
    zmq_pollitem_t pollitem{};
    pollitem.socket = m_subscriber;
    pollitem.events = ZMQ_POLLIN;

    const int event_count{zmq_poll(&pollitem, 1, 50)};
    for (int i{0}; i < event_count; ++i)
    {
      const int read{zmq_recv(m_subscriber, buffer.data(), buffer.size(), 0)};
      if (read < static_cast<int>(sizeof(self)))
        continue;

      u64 id{};
      std::memcpy(&id, buffer.data(), sizeof(self));

      const bool self_message{id == self};
      if (self_message)
        continue;

      if (!m_read_enabled.IsSet())
        continue;

      const u8* const frame{buffer.data() + sizeof(self)};
      const u64 size{read - sizeof(self)};

      std::memcpy(m_eth_ref->mRecvBuffer.get(), frame, size);
      m_eth_ref->mRecvBufferLength = static_cast<u32>(size);
      m_eth_ref->RecvHandlePacket();
    }
  }
}

void CEXIETHERNET::IPCBBAInterface::ProxyThreadHandler()
{
  while (!m_proxy_thread_shutdown.IsSet())
  {
    // The first instance that acquires the filelock becomes the proxy.
    const auto filelock = file_lock::FileLockFactory::CreateTimedLockContext(
        "/tmp/dolphin-bba-filelock", std::chrono::seconds(1));
    if (!filelock)
      continue;

    void* proxy_subscriber;
    void* proxy_publisher;

    {
      std::lock_guard<std::mutex> lock{m_proxy_mutex};

      const u32 linger{0};

      m_proxy_subscriber = zmq_socket(m_context, ZMQ_XSUB);
      zmq_setsockopt(m_proxy_subscriber, ZMQ_LINGER, &linger, sizeof(linger));
      zmq_setsockopt(m_proxy_subscriber, ZMQ_SUBSCRIBE, "", 0);
      zmq_bind(m_proxy_subscriber, "ipc:///tmp/dolphin-bba-outbox");

      m_proxy_publisher = zmq_socket(m_context, ZMQ_XPUB);
      zmq_setsockopt(m_proxy_publisher, ZMQ_LINGER, &linger, sizeof(linger));
      zmq_bind(m_proxy_publisher, "ipc:///tmp/dolphin-bba-inbox");

      proxy_subscriber = m_proxy_subscriber;
      proxy_publisher = m_proxy_publisher;
    }

    zmq_proxy(proxy_subscriber, proxy_publisher, nullptr);
  }
}
}  // namespace ExpansionInterface
