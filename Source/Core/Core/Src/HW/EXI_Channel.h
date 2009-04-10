// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _EXICHANNEL_H
#define _EXICHANNEL_H

#include "Common.h"

#include "EXI_Device.h"

#ifdef _WIN32
#pragma warning(disable:4201)
#endif

class CEXIChannel
{
private:

	enum
	{
		EXI_STATUS		= 0,
		EXI_DMAADDR		= 1,
		EXI_DMALENGTH	= 2,
		EXI_DMACONTROL	= 3,
		EXI_IMMDATA		= 4
	};

	// EXI Status Register
	union UEXI_STATUS
	{
		u32 hex;
		struct
		{
			unsigned EXIINTMASK		: 1; //31
			unsigned EXIINT			: 1; //30
			unsigned TCINTMASK		: 1; //29
			unsigned TCINT			: 1; //28
			unsigned CLK			: 3; //27
			unsigned CHIP_SELECT	: 3; //24
			unsigned EXTINTMASK		: 1; //21
			unsigned EXTINT			: 1; //20
			unsigned EXT			: 1; //19 // External Insertion Status (1: External EXI device present)
			unsigned ROMDIS			: 1; //18 // ROM Disable
			unsigned				:18;
		};  // DO NOT obey the warning and give this struct a name. Things will fail.
		UEXI_STATUS() {hex = 0;}
		UEXI_STATUS(u32 _hex) {hex = _hex;}
	};

	// EXI Control Register
	union UEXI_CONTROL
	{
		u32 hex;
		struct
		{
			unsigned TSTART			: 1;
			unsigned DMA			: 1;
			unsigned RW				: 2;
			unsigned TLEN			: 2;
			unsigned				:26;
		};
	};

	// STATE_TO_SAVE
	UEXI_STATUS		m_Status;
	UEXI_CONTROL	m_Control;
	u32				m_DMAMemoryAddress;
	u32				m_DMALength;
	u32				m_ImmData;

	// get device
	IEXIDevice* GetDevice(u8 _CHIP_SELECT);

	// Devices
	enum
	{
		NUM_DEVICES = 3
	};

	IEXIDevice* m_pDevices[NUM_DEVICES];

public:
	// channelId for debugging
	u32 m_ChannelId;

	CEXIChannel();
	~CEXIChannel();

	void AddDevice(const TEXIDevices _device, const unsigned int _iSlot);

	// Remove all devices
	void RemoveDevices();

	void Read32(u32& _uReturnValue, const u32 _iRegister);
	void Write32(const u32 _iValue, const u32 _iRegister);

	void Update();
	bool IsCausingInterrupt();
	void UpdateInterrupts();
};

#endif
