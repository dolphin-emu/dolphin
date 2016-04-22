// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI_DeviceGCAdapter.h"
#include "InputCommon/GCAdapter.h"

CSIDevice_GCAdapter::CSIDevice_GCAdapter(SIDevices device, int _iDeviceNumber)
	: CSIDevice_GCController(device, _iDeviceNumber)
{
	// get the correct pad number that should rumble locally when using netplay
	const u8 numPAD = NetPlay_InGamePadToLocalPad(ISIDevice::m_iDeviceNumber);
	if (numPAD < 4)
		m_simulate_konga = SConfig::GetInstance().m_AdapterKonga[numPAD];
}

GCPadStatus CSIDevice_GCAdapter::GetPadStatus()
{
	GCPadStatus PadStatus;
	memset(&PadStatus, 0, sizeof(PadStatus));

	GCAdapter::Input(ISIDevice::m_iDeviceNumber, &PadStatus);

	HandleMoviePadStatus(&PadStatus);

	return PadStatus;
}

void CSIDevice_GCController::Rumble(u8 numPad, ControlState strength)
{
	SIDevices device = SConfig::GetInstance().m_SIDevice[numPad];
	if (device == SIDEVICE_WIIU_ADAPTER)
		GCAdapter::Output(numPad, static_cast<u8>(strength));
	else if (SIDevice_IsGCController(device))
		Pad::Rumble(numPad, strength);
}
