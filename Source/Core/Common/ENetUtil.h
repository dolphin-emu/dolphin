// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
#pragma once

#include <enet/enet.h>

#include <SFML/Network/Packet.hpp>

#include "Common/CommonTypes.h"

namespace ENetUtil
{
void WakeupThread(ENetHost* host);
int ENET_CALLBACK InterceptCallback(ENetHost* host, ENetEvent* event);
bool SendPacket(ENetPeer* socket, const sf::Packet& packet, u8 channel_id);
}  // namespace ENetUtil
