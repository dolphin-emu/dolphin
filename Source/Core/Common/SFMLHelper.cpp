// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/SFMLHelper.h"

#if !defined(_WIN32)
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <SFML/Network/Packet.hpp>

#include "Common/Network.h"

sf::Packet& operator>>(sf::Packet& packet, Common::BigEndianValue<u16>& data)
{
  u16 tmp;
  packet >> tmp;
  data = tmp;
  return packet;
}

sf::Packet& operator>>(sf::Packet& packet, Common::BigEndianValue<u32>& data)
{
  u32 tmp;
  packet >> tmp;
  data = tmp;
  return packet;
}

sf::Packet& operator>>(sf::Packet& packet, Common::BigEndianValue<u64>& data)
{
  u64 tmp;
  packet >> tmp;
  data = tmp;
  return packet;
}

namespace Common
{
// SFML's Uint64 type is different depending on platform,
// so we have this for cleaner code.
u64 PacketReadU64(sf::Packet& packet)
{
  u64 value;
  packet >> value;
  return value;
}

#if !defined(_WIN32)
sf::Socket::Status ReuseAddrSfTcpListener::listen(u16 port, sf::IpAddress address)
{
  close();
  create();

  // This whole function is effectively just a copy of sf::TcpListener::listen with this line added.
  SetReuseAddressOnUnix(getNativeHandle());

  if (address == sf::IpAddress::Broadcast)
    return Status::Error;

  sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = std::bit_cast<in_addr>(htonl(address.toInteger())),
  };
  if (bind(getNativeHandle(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
    return Status::Error;

  if (::listen(getNativeHandle(), SOMAXCONN) == -1)
    return Status::Error;

  return Status::Done;
}
#endif
}  // namespace Common
