// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Common.h"
#include "ChunkFile.h"

#include "PeripheralInterface.h"
#include "../PowerPC/PowerPC.h"

#include "EXI_Device.h"
#include "EXI_Channel.h"

namespace ExpansionInterface
{

enum
{
	NUM_CHANNELS = 3
};

CEXIChannel *g_Channels;

void Init()
{
	g_Channels = new CEXIChannel[3];
	g_Channels[0].m_ChannelId = 0;
	g_Channels[1].m_ChannelId = 1;
	g_Channels[2].m_ChannelId = 2;

	g_Channels[0].AddDevice(EXIDEVICE_MEMORYCARD_A,	0);
	g_Channels[0].AddDevice(EXIDEVICE_IPL,			1);
	g_Channels[1].AddDevice(EXIDEVICE_MEMORYCARD_B,	0);
	g_Channels[2].AddDevice(EXIDEVICE_AD16,			0);
}

void Shutdown()
{
	delete [] g_Channels;
	g_Channels = 0;
}

void DoState(ChunkFile &f)
{
	f.Descend("EXI ");
	// TODO: descend all the devices recursively.
	f.Ascend();
}

void Update()
{
	g_Channels[0].Update();
	g_Channels[1].Update();
	g_Channels[2].Update();
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	unsigned int iAddr = _iAddress & 0x3FF;
	unsigned int iRegister = (iAddr >> 2) % 5;
	unsigned int iChannel = (iAddr >> 2) / 5;

	_dbg_assert_(EXPANSIONINTERFACE, iChannel < NUM_CHANNELS);

	if (iChannel < NUM_CHANNELS)
	{
		g_Channels[iChannel].Read32(_uReturnValue, iRegister);
	}
	else
	{
		_uReturnValue = 0;
	}
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	int iAddr = _iAddress & 0x3FF;
	int iRegister = (iAddr >> 2) % 5;
	int iChannel = (iAddr >> 2) / 5;

	_dbg_assert_(EXPANSIONINTERFACE, iChannel < NUM_CHANNELS)

	if (iChannel < NUM_CHANNELS)
		g_Channels[iChannel].Write32(_iValue, iRegister);
}

void UpdateInterrupts()
{
	for(int i=0; i<NUM_CHANNELS; i++)
	{
		if(g_Channels[i].IsCausingInterrupt())
		{
			CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_EXI, true);
			return;
		}
	}

	CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_EXI, false);
}

} // end of namespace ExpansionInterface
