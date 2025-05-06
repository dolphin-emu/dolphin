// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <SFML/Network/Packet.hpp>

#include "Common/CommonTypes.h"
#include "Common/EnumUtils.h"
#include "Common/Swap.h"
#include "Common/TypeUtils.h"

sf::Packet& operator>>(sf::Packet& packet, Common::BigEndianValue<u16>& data);
sf::Packet& operator>>(sf::Packet& packet, Common::BigEndianValue<u32>& data);
sf::Packet& operator>>(sf::Packet& packet, Common::BigEndianValue<u64>& data);

template <Common::Enum Enum>
sf::Packet& operator<<(sf::Packet& packet, Enum e)
{
  packet << Common::ToUnderlying(e);
  return packet;
}

template <Common::Enum Enum>
sf::Packet& operator>>(sf::Packet& packet, Enum& e)
{
  using Underlying = std::underlying_type_t<Enum>;

  Underlying value{};
  packet >> value;

  e = static_cast<Enum>(value);
  return packet;
}

namespace Common
{
u64 PacketReadU64(sf::Packet& packet);
}  // namespace Common
