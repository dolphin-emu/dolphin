// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/NandPaths.h"
#include "Common/SettingsHandler.h"
#include "Core/ec_wii.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/Net/WII_IPC_HLE_Device_net_kd_request.h"
#include "Core/IPC_HLE/Net/WII_Socket.h"

// Handle /dev/net/kd/request requests
CWII_IPC_HLE_Device_net_kd_request::CWII_IPC_HLE_Device_net_kd_request(u32 device_id, const std::string& name)
	: IWII_IPC_HLE_Device(device_id, name)
{
}

CWII_IPC_HLE_Device_net_kd_request::~CWII_IPC_HLE_Device_net_kd_request()
{
	WiiSockMan::GetInstance().Clean();
}

IPCCommandResult CWII_IPC_HLE_Device_net_kd_request::Open(u32 command_address, u32)
{
	INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: Open");
	Memory::Write_U32(GetDeviceID(), command_address + 4);
	m_Active = true;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_net_kd_request::Close(u32 command_address, bool force)
{
	INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: Close");

	if (!force)
		Memory::Write_U32(0, command_address + 4);

	m_Active = false;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_net_kd_request::IOCtl(u32 command_address)
{
	u32 parameter       = Memory::Read_U32(command_address + 0xC);
	u32 buffer_in       = Memory::Read_U32(command_address + 0x10);
	u32 buffer_in_size  = Memory::Read_U32(command_address + 0x14);
	u32 buffer_out      = Memory::Read_U32(command_address + 0x18);
	u32 buffer_out_size = Memory::Read_U32(command_address + 0x1C);

	u32 return_value = 0;
	switch (parameter)
	{
	case IOCTL_NWC24_SUSPEND_SCHEDULAR:
		// NWC24iResumeForCloseLib  from NWC24SuspendScheduler (Input: none, Output: 32 bytes)
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_SUSPEND_SCHEDULAR - NI");
		Memory::Write_U32(0, buffer_out); // no error
		break;

	case IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR: // NWC24iResumeForCloseLib
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR - NI");
		break;

	case IOCTL_NWC24_EXEC_RESUME_SCHEDULAR: // NWC24iResumeForCloseLib
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_EXEC_RESUME_SCHEDULAR - NI");
		Memory::Write_U32(0, buffer_out); // no error
		break;

	case IOCTL_NWC24_STARTUP_SOCKET: // NWC24iStartupSocket
		Memory::Write_U32(0, buffer_out);
		Memory::Write_U32(0, buffer_out + 4);
		return_value = 0;
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_STARTUP_SOCKET - NI");
		break;

	case IOCTL_NWC24_CLEANUP_SOCKET:
		Memory::Memset(buffer_out, 0, buffer_out_size);
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_CLEANUP_SOCKET - NI");
		break;

	case IOCTL_NWC24_LOCK_SOCKET: // WiiMenu
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_LOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_UNLOCK_SOCKET:
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_UNLOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_REQUEST_REGISTER_USER_ID:
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_REGISTER_USER_ID");
		Memory::Write_U32(0, buffer_out);
		Memory::Write_U32(0, buffer_out + 4);
		break;

	case IOCTL_NWC24_REQUEST_GENERATED_USER_ID: // (Input: none, Output: 32 bytes)
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_GENERATED_USER_ID");
		if (m_config.CreationStage() == NWC24Config::NWC24_IDCS_INITIAL)
		{
			std::string settings_filename(Common::GetTitleDataPath(TITLEID_SYSMENU, Common::FROM_SESSION_ROOT) + WII_SETTING);
			SettingsHandler gen;
			std::string area, model;
			bool got_settings = false;

			if (File::Exists(settings_filename))
			{
				File::IOFile settings_file(settings_filename, "rb");
				if (settings_file.ReadBytes((void*)gen.GetData(), SettingsHandler::SETTINGS_SIZE))
				{
					gen.Decrypt();
					area = gen.GetValue("AREA");
					model = gen.GetValue("MODEL");
					got_settings = true;
				}
			}
			if (got_settings)
			{
				u8 area_code = GetAreaCode(area);
				u8 id_ctr = m_config.IdGen();
				u8 hardware_model = GetHardwareModel(model);

				EcWii& ec = EcWii::GetInstance();
				u32 hollywood_id = ec.getNgId();
				u64 user_id = 0;

				s32 ret = NWC24MakeUserID(&user_id, hollywood_id, id_ctr, hardware_model, area_code);
				m_config.SetId(user_id);
				m_config.IncrementIdGen();
				m_config.SetCreationStage(NWC24Config::NWC24_IDCS_GENERATED);
				m_config.WriteConfig();

				Memory::Write_U32(ret, buffer_out);
			}
			else
			{
				Memory::Write_U32(WC24_ERR_FATAL, buffer_out);
			}
		}
		else if (m_config.CreationStage() == NWC24Config::NWC24_IDCS_GENERATED)
		{
			Memory::Write_U32(WC24_ERR_ID_GENERATED, buffer_out);
		}
		else if (m_config.CreationStage() == NWC24Config::NWC24_IDCS_REGISTERED)
		{
			Memory::Write_U32(WC24_ERR_ID_REGISTERED, buffer_out);
		}
		Memory::Write_U64(m_config.Id(), buffer_out + 4);
		Memory::Write_U32(m_config.CreationStage(), buffer_out + 0xC);
		break;

	case IOCTL_NWC24_GET_SCHEDULAR_STAT:
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_GET_SCHEDULAR_STAT - NI");
		break;

	case IOCTL_NWC24_SAVE_MAIL_NOW:
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_SAVE_MAIL_NOW - NI");
		break;

	case IOCTL_NWC24_REQUEST_SHUTDOWN:
		// if ya set the IOS version to a very high value this happens ...
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_SHUTDOWN - NI");
		break;

	default:
		INFO_LOG(WII_IPC_WC24, "/dev/net/kd/request::IOCtl request 0x%x (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			parameter, buffer_in, buffer_in_size, buffer_out, buffer_out_size);
		break;
	}

	Memory::Write_U32(return_value, command_address + 4);
	return GetDefaultReply();
}


u8 CWII_IPC_HLE_Device_net_kd_request::GetAreaCode(const std::string& area)
{
	static std::map<const std::string, u8> regions = {
		{ "JPN", 0 }, { "USA", 1 }, { "EUR", 2 },
		{ "AUS", 2 }, { "BRA", 1 }, { "TWN", 3 },
		{ "ROC", 3 }, { "KOR", 4 }, { "HKG", 5 },
		{ "ASI", 5 }, { "LTN", 1 }, { "SAF", 2 },
		{ "CHN", 6 },
	};

	auto entry_pos = regions.find(area);
	if (entry_pos != regions.end())
		return entry_pos->second;

	// Unknown
	return 7;
}

u8 CWII_IPC_HLE_Device_net_kd_request::GetHardwareModel(const std::string& model)
{
	static std::map<const std::string, u8> models = {
		{ "RVL", MODEL_RVL },
		{ "RVT", MODEL_RVT },
		{ "RVV", MODEL_RVV },
		{ "RVD", MODEL_RVD },
	};

	auto entry_pos = models.find(model);
	if (entry_pos != models.end())
		return entry_pos->second;

	return MODEL_ELSE;
}

static u8 u64_get_byte(u64 value, u8 shift)
{
	return static_cast<u8>(value >> (shift * 8));
}

static u64 u64_insert_byte(u64 value, u8 shift, u8 byte)
{
	u64 mask = 0x00000000000000FFULL << (shift * 8);
	u64 inst = static_cast<u64>(byte) << (shift * 8);
	return (value & ~mask) | inst;
}

s32 CWII_IPC_HLE_Device_net_kd_request::NWC24MakeUserID(u64* nwc24_id, u32 hollywood_id, u16 id_ctr, u8 hardware_model, u8 area_code)
{
	const u8 table2[8]  = {0x1, 0x5, 0x0, 0x4, 0x2, 0x3, 0x6, 0x7};
	const u8 table1[16] = {0x4, 0xB, 0x7, 0x9, 0xF, 0x1, 0xD, 0x3, 0xC, 0x2, 0x6, 0xE, 0x8, 0x0, 0xA, 0x5};

	u64 mix_id = (static_cast<u64>(area_code) << 50) |
	             (static_cast<u64>(hardware_model) << 47) |
	             (static_cast<u64>(hollywood_id) << 15) |
	             (static_cast<u64>(id_ctr) << 10);

	u64 mix_id_copy1 = mix_id;

	int ctr = 0;
	for (ctr = 0; ctr <= 42; ctr++)
	{
		u64 value = mix_id >> (52 - ctr);
		if (value & 1)
		{
			value = 0x0000000000000635ULL << (42 - ctr);
			mix_id ^= value;
		}
	}

	mix_id = (mix_id_copy1 | (mix_id & 0xFFFFFFFFUL)) ^ 0x0000B3B3B3B3B3B3ULL;
	mix_id = (mix_id >> 10) | ((mix_id & 0x3FF) << (11 + 32));

	for (ctr = 0; ctr <= 5; ctr++)
	{
		u8 ret = u64_get_byte(mix_id, ctr);
		u8 foobar = ((table1[(ret >> 4) & 0xF]) << 4) | (table1[ret & 0xF]);
		mix_id = u64_insert_byte(mix_id, ctr, foobar & 0xff);
	}
	u64 mix_id_copy2 = mix_id;

	for (ctr = 0; ctr <= 5; ctr++)
	{
		u8 ret = u64_get_byte(mix_id_copy2, ctr);
		mix_id = u64_insert_byte(mix_id, table2[ctr], ret);
	}

	mix_id &= 0x001FFFFFFFFFFFFFULL;
	mix_id = (mix_id << 1) | ((mix_id >> 52) & 1);

	mix_id ^= 0x00005E5E5E5E5E5EULL;
	mix_id &= 0x001FFFFFFFFFFFFFULL;

	*nwc24_id = mix_id;

	if (mix_id > 9999999999999999ULL)
		return WC24_ERR_FATAL;

	return WC24_OK;
}
