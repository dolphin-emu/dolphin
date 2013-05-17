// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _UCODE_CARD_H
#define _UCODE_CARD_H

#include "UCodes.h"

class CUCode_CARD : public IUCode
{
public:
	CUCode_CARD(DSPHLE *dsp_hle, u32 crc);
	virtual ~CUCode_CARD();
	u32 GetUpdateMs();

	void HandleMail(u32 _uMail);
	void Update(int cycles);
};

#endif

