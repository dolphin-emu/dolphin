// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/Movie.h"
#include "Core/HW/EXI.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/Sram.h"
#include "Core/PowerPC/PowerPC.h"

SRAM g_SRAM;

namespace ExpansionInterface
{

static int changeDevice;
static int updateInterrupts;

static CEXIChannel *g_Channels[MAX_EXI_CHANNELS];
void Init()
{
	InitSRAM();
	for (u32 i = 0; i < MAX_EXI_CHANNELS; i++)
		g_Channels[i] = new CEXIChannel(i);

	if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
	{
		g_Channels[0]->AddDevice(Movie::IsUsingMemcard(0) ? EXIDEVICE_MEMORYCARD : EXIDEVICE_NONE, 0); // SlotA
		g_Channels[1]->AddDevice(Movie::IsUsingMemcard(1) ? EXIDEVICE_MEMORYCARD : EXIDEVICE_NONE, 0); // SlotB
	}
	else
	{
		g_Channels[0]->AddDevice(SConfig::GetInstance().m_EXIDevice[0], 0); // SlotA
		g_Channels[1]->AddDevice(SConfig::GetInstance().m_EXIDevice[1], 0); // SlotB
	}
	g_Channels[0]->AddDevice(EXIDEVICE_MASKROM,                         1);
	g_Channels[0]->AddDevice(SConfig::GetInstance().m_EXIDevice[2],     2); // Serial Port 1
	g_Channels[2]->AddDevice(EXIDEVICE_AD16,                            0);

	changeDevice = CoreTiming::RegisterEvent("ChangeEXIDevice", ChangeDeviceCallback);
	updateInterrupts = CoreTiming::RegisterEvent("EXIUpdateInterrupts", UpdateInterruptsCallback);
}

void Shutdown()
{
	for (auto& channel : g_Channels)
	{
		delete channel;
		channel = nullptr;
	}
}

void DoState(PointerWrap &p)
{
	for (auto& channel : g_Channels)
		channel->DoState(p);
}

void PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	for (auto& channel : g_Channels)
		channel->PauseAndLock(doLock, unpauseOnUnlock);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
	for (int i = 0; i < MAX_EXI_CHANNELS; ++i)
	{
		_dbg_assert_(EXPANSIONINTERFACE, g_Channels[i] != nullptr);
		// Each channel has 5 32 bit registers assigned to it. We offset the
		// base that we give to each channel for registration.
		//
		// Be careful: this means the base is no longer aligned on a page
		// boundary and using "base | FOO" is not valid!
		g_Channels[i]->RegisterMMIO(mmio, base + 5 * 4 * i);
	}
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

CEXIChannel* GetChannel(u32 index)
{
	return g_Channels[index];
}

IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex)
{
	for (auto& channel : g_Channels)
	{
		IEXIDevice* device = channel->FindDevice(device_type, customIndex);
		if (device)
			return device;
	}
	return nullptr;
}

void UpdateInterrupts()
{
	CoreTiming::ScheduleEvent_Threadsafe(0, updateInterrupts, 0);
}

void UpdateInterruptsCallback(u64 userdata, int cyclesLate)
{
	// Interrupts are mapped a bit strangely:
	// Channel 0 Device 0 generates interrupt on channel 0
	// Channel 0 Device 2 generates interrupt on channel 2
	// Channel 1 Device 0 generates interrupt on channel 1
	g_Channels[2]->SetEXIINT(g_Channels[0]->GetDevice(4)->IsInterruptSet());

	bool causeInt = false;
	for (auto& channel : g_Channels)
		causeInt |= channel->IsCausingInterrupt();

	ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_EXI, causeInt);
}

} // end of namespace ExpansionInterface
