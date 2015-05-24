// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/SI_DeviceGCSteeringWheel.h"

CSIDevice_GCSteeringWheel::CSIDevice_GCSteeringWheel(SIDevices device, int _iDeviceNumber)
	: CSIDevice_GCController(device, _iDeviceNumber)
{}

int CSIDevice_GCSteeringWheel::RunBuffer(u8* _pBuffer, int _iLength)
{
	// For debug logging only
	ISIDevice::RunBuffer(_pBuffer, _iLength);

	// Read the command
	EBufferCommands command = static_cast<EBufferCommands>(_pBuffer[3]);

	// Handle it
	switch (command)
	{
	case CMD_RESET:
	case CMD_ID:
		*(u32*)&_pBuffer[0] = SI_GC_STEERING;
		break;

	// DEFAULT
	default:
	{
		return CSIDevice_GCController::RunBuffer(_pBuffer, _iLength);
	}
	break;
	}

	return _iLength;
}

bool CSIDevice_GCSteeringWheel::GetData(u32& _Hi, u32& _Low)
{
	if (m_Mode == 6)
	{
		GCPadStatus PadStatus = GetPadStatus();

		_Hi = (u32)((u8)PadStatus.stickX); // Steering
		_Hi |= 0x800; // Pedal connected flag
		_Hi |= (u32)((u16)(PadStatus.button | PAD_USE_ORIGIN) << 16);

		_Low = (u8)PadStatus.triggerRight;                     // All 8 bits
		_Low |= (u32)((u8)PadStatus.triggerLeft << 8);          // All 8 bits

		// The GC Steering Wheel appears to have combined pedals
		// (both the Accelerate and Brake pedals are mapped to a single axis)
		// We use the stickY axis for the pedals.
		if (PadStatus.stickY < 128)
			_Low |= (u32)((u8)(255 - ((PadStatus.stickY & 0x7f) * 2)) << 16); // All 8 bits (Brake)
		if (PadStatus.stickY >= 128)
			_Low |= (u32)((u8)((PadStatus.stickY & 0x7f) * 2) << 24); // All 8 bits (Accelerate)

		HandleButtonCombos(PadStatus);
	}
	else
	{
		return CSIDevice_GCController::GetData(_Hi, _Low);
	}

	return true;
}

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
