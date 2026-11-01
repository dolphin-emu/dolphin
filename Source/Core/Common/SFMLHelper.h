// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <SFML/Network/Packet.hpp>
#include "SFML/Network/TcpListener.hpp"

#include <utility>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Common/TypeUtils.h"

sf::Packet& operator>>(sf::Packet& packet, Common::BigEndianValue<u16>& data);
sf::Packet& operator>>(sf::Packet& packet, Common::BigEndianValue<u32>& data);
sf::Packet& operator>>(sf::Packet& packet, Common::BigEndianValue<u64>& data);

template <Common::Enum Enum>
sf::Packet& operator<<(sf::Packet& packet, Enum e)
{
  packet << std::to_underlying(e);
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

// This is a sf::TcpListener that sets SO_REUSEADDR on non-Windows.
// This allows binding immediately after a restart while old TCP sockets are still in "TIME_WAIT".
// SFML really doesn't provide a convenient way to set this on their existing TcpListener class.
class ReuseAddrSfTcpListener final : private sf::TcpListener
{
public:
#if !defined(_WIN32)
  sf::Socket::Status listen(u16 port, sf::IpAddress address = sf::IpAddress::Any);
#else
  using TcpListener::listen;
#endif
  using TcpListener::accept;
  using TcpListener::close;
  using TcpListener::getLocalPort;
  using TcpListener::getNativeHandle;
  using TcpListener::setBlocking;
};
}  // namespace Common
