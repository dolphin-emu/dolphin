// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _UCODE_ROM
#define _UCODE_ROM

#include "UCodes.h"

class CUCode_Rom : public IUCode
{
public:
	CUCode_Rom(DSPHLE *dsp_hle, u32 _crc);
	virtual ~CUCode_Rom();
	u32 GetUpdateMs();

	void HandleMail(u32 _uMail);
	void Update(int cycles);

	void DoState(PointerWrap &p);

private:
	struct SUCode
	{
		u32 m_RAMAddress;
		u32 m_Length;
		u32 m_IMEMAddress;
		u32 m_DMEMLength;
		u32 m_StartPC;
	};
	SUCode m_CurrentUCode;
	int m_BootTask_numSteps;

	u32 m_NextParameter;

	void BootUCode();
};

#endif

