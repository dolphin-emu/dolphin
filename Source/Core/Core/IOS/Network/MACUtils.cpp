// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/MACUtils.h"

#include <optional>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

namespace IOS::Net
{
static void SaveMACAddress(const Common::MACAddress& mac)
{
  Config::SetBaseOrCurrent(Config::MAIN_WIRELESS_MAC, Common::MacAddressToString(mac));
  Config::Save();
}

Common::MACAddress GetMACAddress()
{
  // Parse MAC address from config, and generate a new one if it doesn't
  // exist or can't be parsed.
  std::string wireless_mac = Config::Get(Config::MAIN_WIRELESS_MAC);

  if (Core::WantsDeterminism())
    wireless_mac = "12:34:56:78:9a:bc";

  std::optional<Common::MACAddress> mac = Common::StringToMacAddress(wireless_mac);

  if (!mac)
  {
    mac = Common::GenerateMacAddress(Common::MACConsumer::IOS);
    SaveMACAddress(mac.value());
    if (!wireless_mac.empty())
    {
      ERROR_LOG_FMT(IOS_NET,
                    "The MAC provided ({}) is invalid. We have "
                    "generated another one for you.",
                    Common::MacAddressToString(mac.value()));
    }
  }

  INFO_LOG_FMT(IOS_NET, "Using MAC address: {}", Common::MacAddressToString(mac.value()));
  return mac.value();
}
}  // namespace IOS::Net
