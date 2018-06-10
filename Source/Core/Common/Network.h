// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <optional>
#include <string>

#include "Common/CommonTypes.h"

namespace Common
{
enum class MACConsumer
{
  BBA,
  IOS
};

enum
{
  MAC_ADDRESS_SIZE = 6
};

using MACAddress = std::array<u8, MAC_ADDRESS_SIZE>;

MACAddress GenerateMacAddress(MACConsumer type);
std::string MacAddressToString(const MACAddress& mac);
std::optional<MACAddress> StringToMacAddress(const std::string& mac_string);
}  // namespace Common
