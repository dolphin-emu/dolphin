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

#include "Common.h"
#include "ChunkFile.h"
#include "../ConfigManager.h"
#include "../CoreTiming.h"

#include "PeripheralInterface.h"
#include "../PowerPC/PowerPC.h"

#include "EXI.h"

namespace ExpansionInterface
{

static int changeDevice;

enum
{
	NUM_CHANNELS = 3
};

CEXIChannel *g_Channels;

void Init()
{
	g_Channels = new CEXIChannel[NUM_CHANNELS];
	g_Channels[0].m_ChannelId = 0;
	g_Channels[1].m_ChannelId = 1;
	g_Channels[2].m_ChannelId = 2;

	// m_EXIDevice[0] = SlotA
	// m_EXIDevice[1] = SlotB
	// m_EXIDevice[2] = Serial Port 1 (ETH)
	g_Channels[0].AddDevice(SConfig::GetInstance().m_EXIDevice[0],	0);
	g_Channels[0].AddDevice(EXIDEVICE_IPL,							1);
	g_Channels[0].AddDevice(SConfig::GetInstance().m_EXIDevice[2],	2);
	g_Channels[1].AddDevice(SConfig::GetInstance().m_EXIDevice[1],	0);
	g_Channels[2].AddDevice(EXIDEVICE_AD16,							0);

	changeDevice = CoreTiming::RegisterEvent("ChangeEXIDevice", ChangeDeviceCallback);
}

void Shutdown()
{
	delete [] g_Channels;
	g_Channels = 0;
}

void DoState(PointerWrap &p)
{
	// TODO: descend all the devices recursively.
}

void ChangeDeviceCallback(u64 userdata, int cyclesLate)
{
	u8 channel = (u8)(userdata >> 32);
	u8 device = (u8)(userdata >> 16);
	u8 slot = (u8)userdata;

	g_Channels[channel].AddDevice((TEXIDevices)device, slot);
}

void ChangeDevice(u8 channel, TEXIDevices device, u8 slot)
{
	// Called from GUI, so we need to make it thread safe.
	// Let the hardware see no device for .5b cycles
	CoreTiming::ScheduleEvent_Threadsafe(0, changeDevice, ((u64)channel << 32) | ((u64)EXIDEVICE_DUMMY << 16) | slot);
	CoreTiming::ScheduleEvent_Threadsafe(500000000, changeDevice, ((u64)channel << 32) | ((u64)device << 16) | slot);
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
