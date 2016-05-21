// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/Net/WiiNetConfig.h"

WiiNetConfig::WiiNetConfig()
{
	m_path = File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/" WII_SYSCONF_DIR "/net/02/config.dat";
	ReadConfig();
}

void WiiNetConfig::ResetConfig()
{
	if (File::Exists(m_path))
		File::Delete(m_path);

	memset(&m_config, 0, sizeof(m_config));
	m_config.connType = NetworkConfig::IF_WIRED;
	m_config.connection[0].flags =
		NetworkConnection::WIRED_IF |
		NetworkConnection::DNS_DHCP |
		NetworkConnection::IP_DHCP |
		NetworkConnection::CONNECTION_TEST_OK |
		NetworkConnection::CONNECTION_SELECTED;

	WriteConfig();
}

void WiiNetConfig::WriteConfig()
{
	if (!File::Exists(m_path))
	{
		if (!File::CreateFullPath(std::string(File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/" WII_SYSCONF_DIR "/net/02/")))
		{
			ERROR_LOG(WII_IPC_NET, "Failed to create directory for network config file");
		}
	}

	File::IOFile(m_path, "wb").WriteBytes(static_cast<void*>(&m_config), sizeof(m_config));
}

void WiiNetConfig::ReadConfig()
{
	if (File::Exists(m_path))
	{
		if (!File::IOFile(m_path, "rb").ReadBytes(static_cast<void*>(&m_config), sizeof(m_config)))
			ResetConfig();
	}
	else
	{
		ResetConfig();
	}
}

void WiiNetConfig::WriteToMem(u32 address) const
{
	Memory::CopyToEmu(address, &m_config, sizeof(m_config));
}

void WiiNetConfig::ReadFromMem(u32 address)
{
	Memory::CopyFromEmu(&m_config, address, sizeof(m_config));
}
