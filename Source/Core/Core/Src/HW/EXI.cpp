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

#include "ProcessorInterface.h"
#include "../PowerPC/PowerPC.h"

#include "EXI.h"

namespace ExpansionInterface
{

static int changeDevice;

enum
{
	NUM_CHANNELS = 3
};

CEXIChannel *g_Channels[NUM_CHANNELS];

void Init()
{
	for (u32 i = 0; i < NUM_CHANNELS; i++)
		g_Channels[i] = new CEXIChannel(i);

	g_Channels[0]->AddDevice(SConfig::GetInstance().m_EXIDevice[0],	0); // SlotA
	g_Channels[0]->AddDevice(EXIDEVICE_MASKROM,						1);
	g_Channels[0]->AddDevice(SConfig::GetInstance().m_EXIDevice[2],	2); // Serial Port 1
	g_Channels[1]->AddDevice(SConfig::GetInstance().m_EXIDevice[1],	0); // SlotB
	g_Channels[2]->AddDevice(EXIDEVICE_AD16,						0);

	changeDevice = CoreTiming::RegisterEvent("ChangeEXIDevice", ChangeDeviceCallback);
}

void Shutdown()
{
	for (u32 i = 0; i < NUM_CHANNELS; i++)
	{
		delete g_Channels[i];
		g_Channels[i] = NULL;
	}
}

void DoState(PointerWrap &p)
{
	// TODO: Complete DoState for each IEXIDevice
	g_Channels[0]->GetDevice(1)->DoState(p);
	g_Channels[0]->GetDevice(2)->DoState(p);
	g_Channels[0]->GetDevice(4)->DoState(p);
	g_Channels[1]->GetDevice(1)->DoState(p);	
	g_Channels[2]->GetDevice(1)->DoState(p);
}

void ChangeDeviceCallback(u64 userdata, int cyclesLate)
{
	u8 channel = (u8)(userdata >> 32);
	u8 device = (u8)(userdata >> 16);
	u8 slot = (u8)userdata;

	g_Channels[channel]->AddDevice((TEXIDevices)device, slot);
}

void ChangeDevice(u8 channel, TEXIDevices device, u8 slot)
{
	// Called from GUI, so we need to make it thread safe.
	// Let the hardware see no device for .5b cycles
	CoreTiming::ScheduleEvent_Threadsafe(0, changeDevice, ((u64)channel << 32) | ((u64)EXIDEVICE_NONE << 16) | slot);
	CoreTiming::ScheduleEvent_Threadsafe(500000000, changeDevice, ((u64)channel << 32) | ((u64)device << 16) | slot);
}

// Unused (?!)
void Update()
{
	g_Channels[0]->Update();
	g_Channels[1]->Update();
	g_Channels[2]->Update();
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	// TODO 0xfff00000 is mapped to EXI -> mapped to first MB of maskrom
	u32 iAddr = _iAddress & 0x3FF;
	u32 iRegister = (iAddr >> 2) % 5;
	u32 iChannel = (iAddr >> 2) / 5;

	_dbg_assert_(EXPANSIONINTERFACE, iChannel < NUM_CHANNELS);

	if (iChannel < NUM_CHANNELS)
	{
		g_Channels[iChannel]->Read32(_uReturnValue, iRegister);
	}
	else
	{
		_uReturnValue = 0;
	}
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	// TODO 0xfff00000 is mapped to EXI -> mapped to first MB of maskrom
	u32 iAddr = _iAddress & 0x3FF;
	u32 iRegister = (iAddr >> 2) % 5;
	u32 iChannel = (iAddr >> 2) / 5;

	_dbg_assert_(EXPANSIONINTERFACE, iChannel < NUM_CHANNELS);

	if (iChannel < NUM_CHANNELS)
		g_Channels[iChannel]->Write32(_iValue, iRegister);
}

void UpdateInterrupts()
{
	// Interrupts are mapped a bit strangely:
	// Channel 0 Device 0 generates interrupt on channel 0
	// Channel 0 Device 2 generates interrupt on channel 2
	// Channel 1 Device 0 generates interrupt on channel 1
	g_Channels[2]->SetEXIINT(g_Channels[0]->GetDevice(4)->IsInterruptSet());

	bool causeInt = false;
	for (int i = 0; i < NUM_CHANNELS; i++)
		causeInt |= g_Channels[i]->IsCausingInterrupt();

	ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_EXI, causeInt);
}

} // end of namespace ExpansionInterface
