// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/HW/GCPad.h"
#include "Core/HW/SI_DeviceGCSteeringWheel.h"

CSIDevice_GCSteeringWheel::CSIDevice_GCSteeringWheel(SIDevices device, int _iDeviceNumber)
	: CSIDevice_GCController(device, _iDeviceNumber)
{}

void CSIDevice_GCSteeringWheel::SendCommand(u32 _Cmd, u8 _Poll)
{
	UCommand command(_Cmd);

	if (command.Command == CMD_FORCE)
	{
		unsigned int uStrength = command.Parameter1; // 0 = left strong, 127 = left weak, 128 = right weak, 255 = right strong
		unsigned int uType = command.Parameter2;  // 06 = motor on, 04 = motor off

		// get the correct pad number that should rumble locally when using netplay
		const u8 numPAD = NetPlay_InGamePadToLocalPad(ISIDevice::m_iDeviceNumber);

		if (numPAD < 4)
		{
			if (uType == 0x06)
			{
				// map 0..255 to -1.0..1.0
				ControlState strength = uStrength / 127.5 - 1;
				Pad::Rumble(numPAD, strength);
			}
			else
			{
				Pad::Rumble(numPAD, 0);
			}
		}

		if (!_Poll)
		{
			m_Mode = command.Parameter2;
			INFO_LOG(SERIALINTERFACE, "PAD %i set to mode %i", ISIDevice::m_iDeviceNumber, m_Mode);
		}
	}
	else
	{
		return CSIDevice_GCController::SendCommand(_Cmd, _Poll);
	}
}
