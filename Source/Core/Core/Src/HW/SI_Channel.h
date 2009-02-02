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
#ifndef _SICHANNEL_H
#define _SICHANNEL_H

#include "Common.h"

#include "SI_Device.h"

class CSIChannel
{
private:
	// SI Internal Hardware Addresses
	enum
	{
		SI_CHANNEL_0_OUT	= 0x00,
		SI_CHANNEL_0_IN_HI	= 0x04,
		SI_CHANNEL_0_IN_LO	= 0x08,
		SI_CHANNEL_1_OUT	= 0x0C,
		SI_CHANNEL_1_IN_HI	= 0x10,
		SI_CHANNEL_1_IN_LO	= 0x14,
		SI_CHANNEL_2_OUT	= 0x18,
		SI_CHANNEL_2_IN_HI	= 0x1C,
		SI_CHANNEL_2_IN_LO	= 0x20,
		SI_CHANNEL_3_OUT	= 0x24,
		SI_CHANNEL_3_IN_HI	= 0x28,
		SI_CHANNEL_3_IN_LO	= 0x2C,
		SI_POLL				= 0x30,
		SI_COM_CSR			= 0x34,
		SI_STATUS_REG		= 0x38,
		SI_EXI_CLOCK_COUNT	= 0x3C,
	};

	// SI Channel Output
	union USIChannelOut
	{
		u32 Hex;
		struct
		{
			unsigned OUTPUT1	:	8;
			unsigned OUTPUT0	:	8;
			unsigned CMD		:	8;
			unsigned			:	8;
		};
	};

	// SI Channel Input High u32
	union USIChannelIn_Hi
	{
		u32 Hex;
		struct
		{
			unsigned INPUT3		:	8;
			unsigned INPUT2		:	8;
			unsigned INPUT1		:	8;
			unsigned INPUT0		:	6;
			unsigned ERRLATCH	:	1; // 0: no error  1: Error latched. Check SISR.
			unsigned ERRSTAT	:	1; // 0: no error  1: error on last transfer
		};
	};

	// SI Channel Input Low u32
	union USIChannelIn_Lo
	{
		u32 Hex;
		struct
		{
			unsigned INPUT7		:	8;
			unsigned INPUT6		:	8;
			unsigned INPUT5		:	8;
			unsigned INPUT4		:	8;
		};
	};
public: // HAX
	// SI Channel
	USIChannelOut	m_Out;
	USIChannelIn_Hi m_InHi;
	USIChannelIn_Lo m_InLo;
	ISIDevice*		m_pDevice;

//public:
	// ChannelId for debugging
	u32 m_ChannelId;

	CSIChannel();
	~CSIChannel();

	void AddDevice(const TSIDevices _device, int _iSlot);

	// Remove the device
	void RemoveDevice();

	void Read32(u32& _uReturnValue, const u32 _iRegister);
	void Write32(const u32 _iValue, const u32 _iRegister);
};

#endif
