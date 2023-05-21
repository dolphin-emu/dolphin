// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#include "UICommon/Steam/HelperClient.h"

namespace Steam
{
std::future<IpcResult> HelperClient::SendMessageWithReply(MessageType type,
                                                          const sf::Packet& payload)
{
  auto promise = std::make_shared<std::promise<IpcResult>>();

  sf::Packet packet;
  packet << static_cast<uint8_t>(type);

  {
    std::scoped_lock lock(m_promises_mutex);

    packet << ++m_last_call_id;

    m_promises[m_last_call_id] = promise;
  }

  packet.append(payload.getData(), payload.getDataSize());

  Send(packet);

  return promise->get_future();
}

void HelperClient::SendMessageNoReply(MessageType type, const sf::Packet& payload)
{
  sf::Packet packet;
  packet << static_cast<uint8_t>(type);
  packet << std::numeric_limits<uint32_t>::max();  // dummy call ID

  packet.append(payload.getData(), payload.getDataSize());

  Send(packet);
}

void HelperClient::Receive(sf::Packet& packet)
{
  uint8_t raw_type;
  packet >> raw_type;

  MessageType type = static_cast<MessageType>(raw_type);

  uint32_t call_id;
  packet >> call_id;

  switch (type)
  {
  case MessageType::InitReply:
  {
    std::scoped_lock lock(m_promises_mutex);

    m_promises[call_id].get()->set_value({true, packet});
    m_promises.erase(call_id);
  }

  break;
  default:
    RequestStop();
    break;
  }
}

void HelperClient::HandleRequestedStop()
{
  std::scoped_lock lock(m_promises_mutex);

  for (auto iter = m_promises.begin(); iter != m_promises.end(); ++iter)
  {
    iter->second.get()->set_value({false, {}});
  }

  m_promises.clear();
}
}  // namespace Steam
