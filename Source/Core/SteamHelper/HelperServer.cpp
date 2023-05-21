// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#include "SteamHelper/HelperServer.h"

#include <steam/steam_api.h>

#include "SteamHelperCommon/Constants.h"
#include "SteamHelperCommon/InitResult.h"
#include "SteamHelperCommon/MessageType.h"

namespace Steam
{
void HelperServer::Receive(sf::Packet& packet)
{
  uint8_t rawType;
  packet >> rawType;

  MessageType type = static_cast<MessageType>(rawType);

  uint32_t call_id;
  packet >> call_id;

  switch (type)
  {
  case MessageType::InitRequest:
    ReceiveInitRequest(call_id);
    break;
  case MessageType::ShutdownRequest:
    RequestStop();
    break;
  default:
    RequestStop();
    break;
  }
}

void HelperServer::ReceiveInitRequest(uint32_t call_id)
{
  sf::Packet replyPacket;

  replyPacket << static_cast<uint8_t>(MessageType::InitReply);
  replyPacket << call_id;

  if (SteamAPI_RestartAppIfNecessary(STEAM_APP_ID))
  {
    replyPacket << static_cast<uint8_t>(InitResult::RestartingFromSteam);
  }
  else if (!SteamAPI_Init())
  {
    replyPacket << static_cast<uint8_t>(InitResult::Failure);
  }
  else
  {
    replyPacket << static_cast<uint8_t>(InitResult::Success);

    m_steam_inited = true;
  }

  Send(replyPacket);
}

void HelperServer::ReceiveShutdownRequest()
{
  RequestStop();
}
}  // namespace Steam
