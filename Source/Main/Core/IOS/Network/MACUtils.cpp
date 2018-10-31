// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/MACUtils.h"

#include <optional>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"

namespace IOS::Net
{
static void SaveMACAddress(const Common::MACAddress& mac)
{
  SConfig::GetInstance().m_WirelessMac = Common::MacAddressToString(mac);
  SConfig::GetInstance().SaveSettings();
}

Common::MACAddress GetMACAddress()
{
  // Parse MAC address from config, and generate a new one if it doesn't
  // exist or can't be parsed.
  std::string wireless_mac = SConfig::GetInstance().m_WirelessMac;

  if (Core::WantsDeterminism())
    wireless_mac = "12:34:56:78:9a:bc";

  std::optional<Common::MACAddress> mac = Common::StringToMacAddress(wireless_mac);

  if (!mac)
  {
    mac = Common::GenerateMacAddress(Common::MACConsumer::IOS);
    SaveMACAddress(mac.value());
    if (!wireless_mac.empty())
    {
      ERROR_LOG(IOS_NET,
                "The MAC provided (%s) is invalid. We have "
                "generated another one for you.",
                Common::MacAddressToString(mac.value()).c_str());
    }
  }

  INFO_LOG(IOS_NET, "Using MAC address: %s", Common::MacAddressToString(mac.value()).c_str());
  return mac.value();
}
}  // namespace IOS::Net
