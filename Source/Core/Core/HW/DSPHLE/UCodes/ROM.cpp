// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/HW/DSPHLE/UCodes/ROM.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

ROMUCode::ROMUCode(DSPHLE *dsphle, u32 crc)
	: UCodeInterface(dsphle, crc)
	, m_current_ucode()
	, m_boot_task_num_steps(0)
	, m_next_parameter(0)
{
	DEBUG_LOG(DSPHLE, "UCode_Rom - initialized");
	m_mail_handler.Clear();
	m_mail_handler.PushMail(0x8071FEED);
}

ROMUCode::~ROMUCode()
{
}

void ROMUCode::Update()
{
}

void ROMUCode::HandleMail(u32 mail)
{
	if (m_next_parameter == 0)
	{
		// wait for beginning of UCode
		if ((mail & 0xFFFF0000) != 0x80F30000)
		{
			u32 Message = 0xFEEE0000 | (mail & 0xFFFF);
			m_mail_handler.PushMail(Message);
		}
		else
		{
			m_next_parameter = mail;
		}
	}
	else
	{
		switch (m_next_parameter)
		{
		case 0x80F3A001:
			m_current_ucode.m_ram_address = mail;
			break;

		case 0x80F3A002:
			m_current_ucode.m_length = mail & 0xffff;
			break;

		case 0x80F3B002:
			m_current_ucode.m_dmem_length = mail & 0xffff;
			if (m_current_ucode.m_dmem_length)
			{
				NOTICE_LOG(DSPHLE, "m_current_ucode.m_dmem_length = 0x%04x.", m_current_ucode.m_dmem_length);
			}
			break;

		case 0x80F3C002:
			m_current_ucode.m_imem_address = mail & 0xffff;
			break;

		case 0x80F3D001:
			m_current_ucode.m_start_pc = mail & 0xffff;
			BootUCode();
			return;  // Important! BootUCode indirectly does "delete this;". Must exit immediately.

		default:
			break;
		}

		// THE GODDAMN OVERWRITE WAS HERE. Without the return above, since BootUCode may delete "this", well ...
		m_next_parameter = 0;
	}
}

void ROMUCode::BootUCode()
{
	u32 ector_crc = HashEctor(
		(u8*)HLEMemory_Get_Pointer(m_current_ucode.m_ram_address),
		m_current_ucode.m_length);

	if (SConfig::GetInstance().m_DumpUCode)
	{
		std::string ucode_dump_path = StringFromFormat(
			"%sDSP_UC_%08X.bin", File::GetUserPath(D_DUMPDSP_IDX).c_str(), ector_crc);

		File::IOFile fp(ucode_dump_path, "wb");
		if (fp)
		{
			fp.WriteArray((u8*)HLEMemory_Get_Pointer(m_current_ucode.m_ram_address),
						  m_current_ucode.m_length);
		}
	}

	DEBUG_LOG(DSPHLE, "CurrentUCode SOURCE Addr: 0x%08x", m_current_ucode.m_ram_address);
	DEBUG_LOG(DSPHLE, "CurrentUCode Length:      0x%08x", m_current_ucode.m_length);
	DEBUG_LOG(DSPHLE, "CurrentUCode DEST Addr:   0x%08x", m_current_ucode.m_imem_address);
	DEBUG_LOG(DSPHLE, "CurrentUCode DMEM Length: 0x%08x", m_current_ucode.m_dmem_length);
	DEBUG_LOG(DSPHLE, "CurrentUCode init_vector: 0x%08x", m_current_ucode.m_start_pc);
	DEBUG_LOG(DSPHLE, "CurrentUCode CRC:         0x%08x", ector_crc);
	DEBUG_LOG(DSPHLE, "BootTask - done");

	m_dsphle->SetUCode(ector_crc);
}

void ROMUCode::DoState(PointerWrap &p)
{
	p.Do(m_current_ucode);
	p.Do(m_boot_task_num_steps);
	p.Do(m_next_parameter);

	DoStateShared(p);
}

