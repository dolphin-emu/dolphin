// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Core/IPC_HLE/Net/NWC24Config.h"

NWC24Config::NWC24Config()
{
	m_path = File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/" WII_WC24CONF_DIR "/nwc24msg.cfg";
	ReadConfig();
}

void NWC24Config::ResetConfig()
{
	if (File::Exists(m_path))
		File::Delete(m_path);

	const char* urls[5] = {
		"https://amw.wc24.wii.com/cgi-bin/account.cgi",
		"http://rcw.wc24.wii.com/cgi-bin/check.cgi",
		"http://mtw.wc24.wii.com/cgi-bin/receive.cgi",
		"http://mtw.wc24.wii.com/cgi-bin/delete.cgi",
		"http://mtw.wc24.wii.com/cgi-bin/send.cgi",
	};

	memset(&m_config, 0, sizeof(m_config));

	SetMagic(0x57634366);
	SetUnk(8);
	SetCreationStage(NWC24_IDCS_INITIAL);
	SetEnableBooting(0);
	SetEmail("@wii.com");

	for (int i = 0; i < NWC24ConfigData::URL_COUNT; ++i)
	{
		strncpy(m_config.http_urls[i], urls[i], NWC24ConfigData::MAX_URL_LENGTH);
	}

	SetChecksum(CalculateNwc24ConfigChecksum());

	WriteConfig();
}

void NWC24Config::WriteConfig()
{
	if (!File::Exists(m_path))
	{
		if (!File::CreateFullPath(File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/" WII_WC24CONF_DIR))
		{
			ERROR_LOG(WII_IPC_WC24, "Failed to create directory for WC24");
		}
	}

	File::IOFile(m_path, "wb").WriteBytes(static_cast<void*>(&m_config), sizeof(m_config));
}

void NWC24Config::ReadConfig()
{
	if (File::Exists(m_path))
	{
		if (!File::IOFile(m_path, "rb").ReadBytes(static_cast<void*>(&m_config), sizeof(m_config)))
			ResetConfig();
		else
		{
			s32 config_error = CheckNwc24Config();
			if (config_error)
				ERROR_LOG(WII_IPC_WC24, "There is an error in the config for for WC24: %d", config_error);
		}
	}
	else
	{
		ResetConfig();
	}
}

u32 NWC24Config::CalculateNwc24ConfigChecksum()
{
	u32* ptr = reinterpret_cast<u32*>(&m_config);
	u32 sum = 0;

	for (int i = 0; i < 0xFF; ++i)
	{
		sum += Common::swap32(*ptr++);
	}

	return sum;
}

s32 NWC24Config::CheckNwc24Config()
{
	if (Magic() != 0x57634366) /* 'WcCf' magic */
	{
		ERROR_LOG(WII_IPC_WC24, "Magic mismatch");
		return -14;
	}

	u32 checksum = CalculateNwc24ConfigChecksum();
	DEBUG_LOG(WII_IPC_WC24, "Checksum: %X", checksum);

	if (Checksum() != checksum)
	{
		ERROR_LOG(WII_IPC_WC24, "Checksum mismatch expected %X and got %X", checksum, Checksum());
		return -14;
	}

	if (IdGen() > 0x1F)
	{
		ERROR_LOG(WII_IPC_WC24, "Id gen error");
		return -14;
	}

	if (Unk() != 8)
		return -27;

	return 0;
}

u32 NWC24Config::Magic() const
{
	return Common::swap32(m_config.magic);
}

void NWC24Config::SetMagic(u32 magic)
{
	m_config.magic = Common::swap32(magic);
}

u32 NWC24Config::Unk() const
{
	return Common::swap32(m_config.unk_04);
}

void NWC24Config::SetUnk(u32 unk_04)
{
	m_config.unk_04 = Common::swap32(unk_04);
}

u32 NWC24Config::IdGen() const
{
	return Common::swap32(m_config.id_generation);
}

void NWC24Config::SetIdGen(u32 id_generation)
{
	m_config.id_generation = Common::swap32(id_generation);
}

void NWC24Config::IncrementIdGen()
{
	u32 id_ctr = IdGen();
	id_ctr++;
	id_ctr &= 0x1F;
	SetIdGen(id_ctr);
}

u32 NWC24Config::Checksum() const
{
	return Common::swap32(m_config.checksum);
}

void NWC24Config::SetChecksum(u32 checksum)
{
	m_config.checksum = Common::swap32(checksum);
}

u32 NWC24Config::CreationStage() const
{
	return Common::swap32(m_config.creation_stage);
}

void NWC24Config::SetCreationStage(u32 creation_stage)
{
	m_config.creation_stage = Common::swap32(creation_stage);
}

u32 NWC24Config::EnableBooting() const
{
	return Common::swap32(m_config.enable_booting);
}

void NWC24Config::SetEnableBooting(u32 enable_booting)
{
	m_config.enable_booting = Common::swap32(enable_booting);
}

u64 NWC24Config::Id() const
{
	return Common::swap64(m_config.nwc24_id);
}

void NWC24Config::SetId(u64 nwc24_id)
{
	m_config.nwc24_id = Common::swap64(nwc24_id);
}

const char* NWC24Config::Email() const
{
	return m_config.email;
}

void NWC24Config::SetEmail(const char* email)
{
	strncpy(m_config.email, email, NWC24ConfigData::MAX_EMAIL_LENGTH);
	m_config.email[NWC24ConfigData::MAX_EMAIL_LENGTH - 1] = '\0';
}
