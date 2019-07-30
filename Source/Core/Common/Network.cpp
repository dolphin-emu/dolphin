// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Network.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>

#include <fmt/format.h>

#include "Common/Random.h"

namespace Common
{
MACAddress GenerateMacAddress(const MACConsumer type)
{
  constexpr std::array<u8, 3> oui_bba{{0x00, 0x09, 0xbf}};
  constexpr std::array<u8, 3> oui_ios{{0x00, 0x17, 0xab}};

  MACAddress mac{};

  switch (type)
  {
  case MACConsumer::BBA:
    std::copy(oui_bba.begin(), oui_bba.end(), mac.begin());
    break;
  case MACConsumer::IOS:
    std::copy(oui_ios.begin(), oui_ios.end(), mac.begin());
    break;
  }

  // Generate the 24-bit NIC-specific portion of the MAC address.
  Random::Generate(&mac[3], 3);
  return mac;
}

std::string MacAddressToString(const MACAddress& mac)
{
  return fmt::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", mac[0], mac[1], mac[2], mac[3],
                     mac[4], mac[5]);
}

std::optional<MACAddress> StringToMacAddress(const std::string& mac_string)
{
  if (mac_string.empty())
    return {};

  int x = 0;
  MACAddress mac{};

  for (size_t i = 0; i < mac_string.size() && x < (MAC_ADDRESS_SIZE * 2); ++i)
  {
    char c = tolower(mac_string.at(i));
    if (c >= '0' && c <= '9')
    {
      mac[x / 2] |= (c - '0') << ((x & 1) ? 0 : 4);
      ++x;
    }
    else if (c >= 'a' && c <= 'f')
    {
      mac[x / 2] |= (c - 'a' + 10) << ((x & 1) ? 0 : 4);
      ++x;
    }
  }

  // A valid 48-bit MAC address consists of 6 octets, where each
  // nibble is a character in the MAC address, making 12 characters
  // in total.
  if (x / 2 != MAC_ADDRESS_SIZE)
    return {};

  return std::make_optional(mac);
}
}  // namespace Common
