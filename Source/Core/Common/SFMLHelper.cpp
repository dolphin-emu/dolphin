// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/SFMLHelper.h"

#include <SFML/Network/Packet.hpp>

namespace Common
{
// This only exists as a helper for BigEndianValue
u16 PacketReadU16(sf::Packet& packet)
{
  u16 tmp;
  packet >> tmp;
  return tmp;
}

// This only exists as a helper for BigEndianValue
u32 PacketReadU32(sf::Packet& packet)
{
  u32 tmp;
  packet >> tmp;
  return tmp;
}

u64 PacketReadU64(sf::Packet& packet)
{
  u32 low, high;
  packet >> low >> high;
  return low | (static_cast<u64>(high) << 32);
}

void PacketWriteU64(sf::Packet& packet, const u64 value)
{
  packet << static_cast<u32>(value) << static_cast<u32>(value >> 32);
}
}  // namespace Common
