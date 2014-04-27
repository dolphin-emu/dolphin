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
	void Update() override;

	void DoState(PointerWrap &p) override;

	u32 Read32()
	{
		u32 res = *(u32*)&m_buffer[m_read_offset];
		m_read_offset += 4;
		return res;
	}

private:
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
