// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/DSPHLE/UCodes/UCodes.h"

struct GBAUCode : public UCodeInterface
{
	GBAUCode(DSPHLE *dsphle, u32 crc);
	virtual ~GBAUCode();
	u32 GetUpdateMs() override;

	void HandleMail(u32 mail) override;
	void Update() override;
};
