// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

class ZeldaUCode : public UCodeInterface
{
public:
	ZeldaUCode(DSPHLE *dsphle, u32 crc);
	virtual ~ZeldaUCode();
	u32 GetUpdateMs() override;

	void HandleMail(u32 mail) override;
	void HandleMail_LightVersion(u32 mail);
	void HandleMail_SMSVersion(u32 mail);
	void HandleMail_NormalVersion(u32 mail);
	void Update() override;

	void DoState(PointerWrap &p) override;

	u32 Read32()
	{
		u32 res = *(u32*)&m_buffer[m_read_offset];
		m_read_offset += 4;
		return res;
	}

private:
	// These map CRC to behavior.

	// DMA version
	// - sound data transferred using DMA instead of accelerator
	bool IsDMAVersion() const
	{
		switch (m_crc)
		{
		case 0xb7eb9a9c: // Wii Pikmin - PAL
		case 0xeaeb38cc: // Wii Pikmin 2 - PAL
		case 0x6c3f6f94: // Wii Zelda TP - PAL
		case 0xD643001F: // Super Mario Galaxy
			return true;
		default:
			return false;
		}
	}

	// Light version
	// - slightly different communication protocol (no list begin mail)
	// - exceptions and interrupts not used
	bool IsLightVersion() const
	{
		switch (m_crc)
		{
		case 0x6ba3b3ea: // IPL - PAL
		case 0x24b22038: // IPL - NTSC/NTSC-JAP
		case 0x42f64ac4: // Luigi's Mansion
		case 0x4be6a5cb: // AC, Pikmin NTSC
			return true;
		default:
			return false;
		}
	}

	// SMS version
	// - sync mails are sent every frame, not every 16 PBs
	// (named SMS because it's used by Super Mario Sunshine
	// and I couldn't find a better name)
	bool IsSMSVersion() const
	{
		switch (m_crc)
		{
		case 0x56d36052: // Super Mario Sunshine
		case 0x267fd05a: // Pikmin PAL
			return true;
		default:
			return false;
		}
	}

	bool m_sync_in_progress;
	u32 m_max_voice;
	u32 m_sync_flags[16];

	// Used by SMS version
	u32 m_num_sync_mail;

	u32 m_num_voices;

	bool m_sync_cmd_pending;
	u32 m_current_voice;
	u32 m_current_buffer;
	u32 m_num_buffers;

	// List, buffer management =====================
	u32 m_num_steps;
	bool m_list_in_progress;
	u32 m_step;
	u8 m_buffer[1024];

	u32 m_read_offset;

	void ExecuteList();
};
