// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/UCodes/GBA.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

GBAUCode::GBAUCode(DSPHLE *dsphle, u32 crc)
: UCodeInterface(dsphle, crc)
{
	m_mail_handler.PushMail(DSP_INIT);
}

GBAUCode::~GBAUCode()
{
	m_mail_handler.Clear();
}

void GBAUCode::Update()
{
	// check if we have to send something
	if (!m_mail_handler.IsEmpty())
	{
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
}

u32 GBAUCode::GetUpdateMs()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bWii ? 3 : 5;
}

void GBAUCode::HandleMail(u32 mail)
{
	static bool nextmail_is_mramaddr = false;
	static bool calc_done = false;

	if (m_upload_setup_in_progress)
	{
		PrepareBootUCode(mail);
	}
	else if ((mail >> 16 == 0xabba) && !nextmail_is_mramaddr)
	{
		nextmail_is_mramaddr = true;
	}
	else if (nextmail_is_mramaddr)
	{
		nextmail_is_mramaddr = false;
		u32 mramaddr = mail;

		struct sec_params_t
		{
			u16 key[2];
			u16 unk1[2];
			u16 unk2[2];
			u32 length;
			u32 dest_addr;
			u32 pad[3];
		} sec_params;

		// 32 bytes from mram addr to DRAM @ 0
		for (int i = 0; i < 8; i++, mramaddr += 4)
			((u32*)&sec_params)[i] = HLEMemory_Read_U32(mramaddr);

		// This is the main decrypt routine
		u16 x11 = 0, x12 = 0,
			x20 = 0, x21 = 0, x22 = 0, x23 = 0;

		x20 = Common::swap16(sec_params.key[0]) ^ 0x6f64;
		x21 = Common::swap16(sec_params.key[1]) ^ 0x6573;

		s16 unk2 = (s8)sec_params.unk2[0];
		if (unk2 < 0)
		{
			x11 = ((~unk2 + 3) << 1) | (sec_params.unk1[0] << 4);
		}
		else if (unk2 == 0)
		{
			x11 = (sec_params.unk1[0] << 1) | 0x70;
		}
		else // unk2 > 0
		{
			x11 = ((unk2 - 1) << 1) | (sec_params.unk1[0] << 4);
		}

		s32 rounded_sub = ((sec_params.length + 7) & ~7) - 0x200;
		u16 size = (rounded_sub < 0) ? 0 : rounded_sub >> 3;

		u32 t = (((size << 16) | 0x3f80) & 0x3f80ffff) << 1;
		s16 t_low = (s8)(t >> 8);
		t += (t_low & size) << 16;
		x12 = t >> 16;
		x11 |= (size & 0x4000) >> 14; // this would be stored in ac0.h if we weren't constrained to 32bit :)
		t = ((x11 & 0xff) << 16) + ((x12 & 0xff) << 16) + (x12 << 8);

		u16 final11 = 0, final12 = 0;
		final11 = x11 | ((t >> 8) & 0xff00) | 0x8080;
		final12 = x12 | 0x8080;

		if ((final12 & 0x200) != 0)
		{
			x22 = final11 ^ 0x6f64;
			x23 = final12 ^ 0x6573;
		}
		else
		{
			x22 = final11 ^ 0x6177;
			x23 = final12 ^ 0x614b;
		}

		// Send the result back to mram
		*(u32*)HLEMemory_Get_Pointer(sec_params.dest_addr) = Common::swap32((x20 << 16) | x21);
		*(u32*)HLEMemory_Get_Pointer(sec_params.dest_addr+4) = Common::swap32((x22 << 16) | x23);

		// Done!
		DEBUG_LOG(DSPHLE, "\n%08x -> key: %08x, len: %08x, dest_addr: %08x, unk1: %08x, unk2: %08x"
			" 22: %04x, 23: %04x",
			mramaddr,
			*(u32*)sec_params.key, sec_params.length, sec_params.dest_addr,
			*(u32*)sec_params.unk1, *(u32*)sec_params.unk2,
			x22, x23);

		calc_done = true;
		m_mail_handler.PushMail(DSP_DONE);
	}
	else if ((mail >> 16 == 0xcdd1) && calc_done)
	{
		switch (mail & 0xffff)
		{
		case 1:
			m_upload_setup_in_progress = true;
			break;
		case 2:
			m_dsphle->SetUCode(UCODE_ROM);
			break;
		default:
			DEBUG_LOG(DSPHLE, "GBAUCode - unknown 0xcdd1 command: %08x", mail);
			break;
		}
	}
	else
	{
		DEBUG_LOG(DSPHLE, "GBAUCode - unknown command: %08x", mail);
	}
}
