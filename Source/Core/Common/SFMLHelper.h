// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace sf
{
class Packet;
}

namespace Common
{
u16 PacketReadU16(sf::Packet& packet);
u32 PacketReadU32(sf::Packet& packet);
u64 PacketReadU64(sf::Packet& packet);
}  // namespace Common
