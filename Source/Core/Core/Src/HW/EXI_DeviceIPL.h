// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _EXIDEVICE_IPL_H
#define _EXIDEVICE_IPL_H

#include "EXI_Device.h"
#include "Sram.h"

class CEXIIPL : public IEXIDevice
{
public:
	CEXIIPL();
	virtual ~CEXIIPL();
	virtual void SetCS(int _iCS);
	bool IsPresent();
    static u32 GetGCTime();

private:

	enum
	{
		ROM_SIZE = 1024*1024*2,
		ROM_MASK = (ROM_SIZE - 1)
	};

	// Region
	bool m_bNTSC;

	//! IPL
	u8* m_pIPL;

	// STATE_TO_SAVE
	//! RealTimeClock
	u8 m_RTC[4];

	//! SRam
	SRAM m_SRAM;

	//! Helper
	u32 m_uPosition;
	u32 m_uAddress;
	u32 m_uRWOffset;

	char m_szBuffer[256];
	int m_count;

	virtual void TransferByte(u8 &_uByte);

	void LoadFileToIPL(const char* filename, u32 offset);
};

#endif

