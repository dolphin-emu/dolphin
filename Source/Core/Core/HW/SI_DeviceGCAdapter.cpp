// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/NetPlayProto.h"
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

	// For netplay, the local controllers are polled in GetNetPads(), and
	// the remote controllers receive their status there as well
	if (!NetPlay::IsNetPlayRunning())
	{
		GCAdapter::Input(ISIDevice::m_iDeviceNumber, &PadStatus);
	}

	HandleMoviePadStatus(&PadStatus);

	return PadStatus;
}

int CSIDevice_GCAdapter::RunBuffer(u8* buffer, int length)
{
	if (!NetPlay::IsNetPlayRunning())
	{
		// The previous check is a hack to prevent a netplay desync due to
		// SI devices being different and returning different values on
		// RunBuffer(); the corresponding code in GCAdapter.cpp has the same
		// check.

		// This returns an error value if there is no controller plugged
		// into this port on the hardware gc adapter, exposing it to the game.
		if (!GCAdapter::DeviceConnected(ISIDevice::m_iDeviceNumber))
		{
			TSIDevices device = SI_NONE;
			memcpy(buffer, &device, sizeof(device));
			return 4;
		}
	}
	return CSIDevice_GCController::RunBuffer(buffer, length);
}

void CSIDevice_GCController::Rumble(u8 numPad, ControlState strength)
{
	SIDevices device = SConfig::GetInstance().m_SIDevice[numPad];
	if (device == SIDEVICE_WIIU_ADAPTER)
		GCAdapter::Output(numPad, static_cast<u8>(strength));
	else if (SIDevice_IsGCController(device))
		Pad::Rumble(numPad, strength);
}
