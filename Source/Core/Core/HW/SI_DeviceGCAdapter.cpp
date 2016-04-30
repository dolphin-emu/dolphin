// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
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

void CSIDevice_GCAdapter::SendCommand(u32 _Cmd, u8 _Poll)
{
	UCommand command(_Cmd);

	switch (command.Command)
	{
	// Costis sent it in some demos :)
	case 0x00:
		break;

	case CMD_WRITE:
		{
			unsigned int uType = command.Parameter1;  // 0 = stop, 1 = rumble, 2 = stop hard
			unsigned int uStrength = command.Parameter2;

			// get the correct pad number that should rumble locally when using netplay
			const u8 numPAD = NetPlay_InGamePadToLocalPad(ISIDevice::m_iDeviceNumber);

			if (numPAD < 4)
			{
				if (uType == 1 && uStrength > 2)
					GCAdapter::Output(numPAD, 1);
				else
					GCAdapter::Output(numPAD, 0);
			}
			if (!_Poll)
			{
				m_Mode = command.Parameter2;
				INFO_LOG(SERIALINTERFACE, "PAD %i set to mode %i", ISIDevice::m_iDeviceNumber, m_Mode);
			}
		}
		break;

	default:
		{
			ERROR_LOG(SERIALINTERFACE, "Unknown direct command     (0x%x)", _Cmd);
			PanicAlert("SI: Unknown direct command");
		}
		break;
	}
}

