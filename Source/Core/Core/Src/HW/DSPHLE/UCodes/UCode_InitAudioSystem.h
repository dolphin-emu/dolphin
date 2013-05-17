// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _UCODE_INITAUDIOSYSTEM
#define _UCODE_INITAUDIOSYSTEM

#include "UCodes.h"

class CUCode_InitAudioSystem : public IUCode
{
public:
	CUCode_InitAudioSystem(DSPHLE *dsp_hle, u32 crc);
	virtual ~CUCode_InitAudioSystem();
	u32 GetUpdateMs();

	void HandleMail(u32 _uMail);
	void Update(int cycles);
	void Init();
};

#endif

