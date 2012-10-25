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
	void DoState(PointerWrap &p);

	static u32 GetGCTime();
	static u32 NetPlay_GetGCTime();

	static void Descrambler(u8* data, u32 size);

private:
	enum
	{
		ROM_SIZE = 1024*1024*2,
		ROM_MASK = (ROM_SIZE - 1)
	};

	enum
	{
		REGION_RTC		= 0x200000,
		REGION_SRAM		= 0x200001,
		REGION_UART		= 0x200100,
		REGION_UART_UNK	= 0x200103,
		REGION_BARNACLE	= 0x200113,
		REGION_WRTC0	= 0x210000,
		REGION_WRTC1	= 0x210001,
		REGION_WRTC2	= 0x210008,
		REGION_EUART_UNK= 0x300000,
		REGION_EUART	= 0x300001
	};

	// Region
	bool m_bNTSC;

	//! IPL
	u8* m_pIPL;

	// STATE_TO_SAVE
	//! RealTimeClock
	u8 m_RTC[4];

	//! Helper
	u32 m_uPosition;
	u32 m_uAddress;
	u32 m_uRWOffset;

	char m_szBuffer[256];
	int m_count;
	bool m_FontsLoaded;

	virtual void TransferByte(u8 &_uByte);
	bool IsWriteCommand() const { return !!(m_uAddress & (1 << 31)); }
	u32 const CommandRegion() const { return (m_uAddress & ~(1 << 31)) >> 8; }

	void LoadFileToIPL(std::string filename, u32 offset);
};

#endif

