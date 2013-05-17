// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "ChunkFile.h"
#include "../ConfigManager.h"
#include "../CoreTiming.h"

#include "ProcessorInterface.h"
#include "../PowerPC/PowerPC.h"

#include "EXI.h"
#include "Sram.h"
#include "../Movie.h"
SRAM g_SRAM;

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
	initSRAM();
	for (u32 i = 0; i < NUM_CHANNELS; i++)
		g_Channels[i] = new CEXIChannel(i);

	if (Movie::IsPlayingInput() && Movie::IsUsingMemcard() && Movie::IsConfigSaved())
		g_Channels[0]->AddDevice(EXIDEVICE_MEMORYCARD,	0); // SlotA
	else if(Movie::IsPlayingInput() && !Movie::IsUsingMemcard() && Movie::IsConfigSaved())
		g_Channels[0]->AddDevice(EXIDEVICE_NONE,		0); // SlotA
	else
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
	for (int c = 0; c < NUM_CHANNELS; ++c)
		g_Channels[c]->DoState(p);
}

void PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	for (int c = 0; c < NUM_CHANNELS; ++c)
		g_Channels[c]->PauseAndLock(doLock, unpauseOnUnlock);
}


void ChangeDeviceCallback(u64 userdata, int cyclesLate)
{
	u8 channel = (u8)(userdata >> 32);
	u8 type = (u8)(userdata >> 16);
	u8 num = (u8)userdata;

	g_Channels[channel]->AddDevice((TEXIDevices)type, num);
}

void ChangeDevice(const u8 channel, const TEXIDevices device_type, const u8 device_num)
{
	// Called from GUI, so we need to make it thread safe.
	// Let the hardware see no device for .5b cycles
	CoreTiming::ScheduleEvent_Threadsafe(0, changeDevice, ((u64)channel << 32) | ((u64)EXIDEVICE_NONE << 16) | device_num);
	CoreTiming::ScheduleEvent_Threadsafe(500000000, changeDevice, ((u64)channel << 32) | ((u64)device_type << 16) | device_num);
}

IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex)
{
	for (int i = 0; i < NUM_CHANNELS; ++i)
	{
		IEXIDevice* device = g_Channels[i]->FindDevice(device_type, customIndex);
		if (device)
			return device;
	}
	return NULL;
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
