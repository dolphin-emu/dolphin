// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/MACUtils.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"

namespace IOS
{
namespace Net
{
static void SaveMACAddress(const u8* mac)
{
  SConfig::GetInstance().m_WirelessMac = Common::MacAddressToString(mac);
  SConfig::GetInstance().SaveSettings();
}

void GetMACAddress(u8* mac)
{
  // Parse MAC address from config, and generate a new one if it doesn't
  // exist or can't be parsed.
  std::string wireless_mac = SConfig::GetInstance().m_WirelessMac;

  if (Core::WantsDeterminism())
    wireless_mac = "12:34:56:78:9a:bc";

  if (!Common::StringToMacAddress(wireless_mac, mac))
  {
    Common::GenerateMacAddress(Common::MACConsumer::IOS, mac);
    SaveMACAddress(mac);
    if (!wireless_mac.empty())
    {
      ERROR_LOG(IOS_NET, "The MAC provided (%s) is invalid. We have "
                         "generated another one for you.",
                Common::MacAddressToString(mac).c_str());
    }
  }

  INFO_LOG(IOS_NET, "Using MAC address: %s", Common::MacAddressToString(mac).c_str());
}
}  // namespace Net
}  // namespace IOS
