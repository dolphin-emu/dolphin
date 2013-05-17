// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "UCodes.h"

struct CUCode_GBA : public IUCode
{
	CUCode_GBA(DSPHLE *dsp_hle, u32 crc);
	virtual ~CUCode_GBA();
	u32 GetUpdateMs();

	void HandleMail(u32 _uMail);
	void Update(int cycles);
};
