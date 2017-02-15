// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cctype>
#include <cstring>
#include <ctime>
#include <random>

#include "Common/Network.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

namespace Common
{
void GenerateMacAddress(const MACConsumer type, u8* mac)
{
  memset(mac, 0, MAC_ADDRESS_SIZE);

  u8 const oui_bba[] = {0x00, 0x09, 0xbf};
  u8 const oui_ios[] = {0x00, 0x17, 0xab};

  switch (type)
  {
  case MACConsumer::BBA:
    memcpy(mac, oui_bba, 3);
    break;
  case MACConsumer::IOS:
    memcpy(mac, oui_ios, 3);
    break;
  }

  // Generate the 24-bit NIC-specific portion of the MAC address.
  std::default_random_engine generator(Common::Timer::GetTimeMs());
  std::uniform_int_distribution<int> distribution(0x00, 0xFF);
  mac[3] = static_cast<u8>(distribution(generator));
  mac[4] = static_cast<u8>(distribution(generator));
  mac[5] = static_cast<u8>(distribution(generator));
}

std::string MacAddressToString(const u8* mac)
{
  return StringFromFormat("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
                          mac[5]);
}

bool StringToMacAddress(const std::string& mac_string, u8* mac)
{
  bool success = false;
  if (!mac_string.empty())
  {
    int x = 0;
    memset(mac, 0, MAC_ADDRESS_SIZE);

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
    success = x / 2 == MAC_ADDRESS_SIZE;
  }
  return success;
}
}  // namespace Common
