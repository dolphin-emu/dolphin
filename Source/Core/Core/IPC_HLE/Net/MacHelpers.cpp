// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Network.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

namespace MacHelpers
{

void SaveMacAddress(u8* mac)
{
	SConfig::GetInstance().m_WirelessMac = MacAddressToString(mac);
	SConfig::GetInstance().SaveSettings();
}

void GetMacAddress(u8* mac)
{
	// Parse MAC address from config, and generate a new one if it doesn't
	// exist or can't be parsed.
	std::string wireless_mac = SConfig::GetInstance().m_WirelessMac;

	if (Core::g_want_determinism)
		wireless_mac = "12:34:56:78:9a:bc";

	if (!StringToMacAddress(wireless_mac, mac))
	{
		GenerateMacAddress(IOS, mac);
		SaveMacAddress(mac);
		if (!wireless_mac.empty())
		{
			ERROR_LOG(WII_IPC_NET, "The MAC provided (%s) is invalid. We have "
			                       "generated another one for you.",
			                       MacAddressToString(mac).c_str());
		}
	}
	INFO_LOG(WII_IPC_NET, "Using MAC address: %s", MacAddressToString(mac).c_str());
}

} // namespace MacHelpers
