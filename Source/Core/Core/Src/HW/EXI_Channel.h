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
	const char* Debug_GetRegisterName(u32 _register)
	{
		switch (_register)
		{
		case EXI_STATUS:		return "STATUS";
		case EXI_DMAADDR:		return "DMAADDR";
		case EXI_DMALENGTH:		return "DMALENGTH";
		case EXI_DMACONTROL:	return "DMACONTROL";
		case EXI_IMMDATA:		return "IMMDATA";
		default:				return "!!! Unknown EXI Register !!!";
		}
	}

	// EXI Status Register - "Channel Parameter Register"
	union UEXI_STATUS
	{
		u32 Hex;
		// DO NOT obey the warning and give this struct a name. Things will fail.
		struct
		{
		// Indentation Meaning:
		//	Channels 0, 1, 2
		//		Channels 0, 1 only
		//			Channel 0 only
			unsigned EXIINTMASK		: 1;
			unsigned EXIINT			: 1;
			unsigned TCINTMASK		: 1;
			unsigned TCINT			: 1;
			unsigned CLK			: 3;
			unsigned CHIP_SELECT	: 3; // CS1 and CS2 are Channel 0 only
				unsigned EXTINTMASK	: 1;
				unsigned EXTINT		: 1;
				unsigned EXT		: 1; // External Insertion Status (1: External EXI device present)
					unsigned ROMDIS	: 1; // ROM Disable
			unsigned				:18;
		};
		UEXI_STATUS() {Hex = 0;}
		UEXI_STATUS(u32 _hex) {Hex = _hex;}
	};

	// EXI Control Register
	union UEXI_CONTROL
	{
		u32 Hex;
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
	u32				m_DMAMemoryAddress;
	u32				m_DMALength;
	UEXI_CONTROL	m_Control;
	u32				m_ImmData;

	// Devices
	enum
	{
		NUM_DEVICES = 3
	};

	IEXIDevice* m_pDevices[NUM_DEVICES];

	// Since channels operate a bit differently from each other
	u32 m_ChannelId;

public:
	// get device
	IEXIDevice* GetDevice(u8 _CHIP_SELECT);

	CEXIChannel(u32 ChannelId);
	~CEXIChannel();

	void AddDevice(const TEXIDevices _device, const unsigned int _iSlot);

	// Remove all devices
	void RemoveDevices();

	void Read32(u32& _uReturnValue, const u32 _iRegister);
	void Write32(const u32 _iValue, const u32 _iRegister);

	void Update();
	bool IsCausingInterrupt();
	void UpdateInterrupts();

	// This should only be used to transition interrupts from SP1 to Channel 2
	void SetEXIINT(bool exiint) { m_Status.EXIINT = !!exiint; }
};

#endif
