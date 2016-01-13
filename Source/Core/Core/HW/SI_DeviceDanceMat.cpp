// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Core/HW/SI_DeviceDanceMat.h"
#include "InputCommon/GCPadStatus.h"

CSIDevice_DanceMat::CSIDevice_DanceMat(SIDevices device, int _iDeviceNumber)
	: CSIDevice_GCController(device, _iDeviceNumber) {}

int CSIDevice_DanceMat::RunBuffer(u8* _pBuffer, int _iLength)
{
	// Read the command
	EBufferCommands command = static_cast<EBufferCommands>(_pBuffer[3]);

	if (command == CMD_RESET)
	{
		ISIDevice::RunBuffer(_pBuffer, _iLength);
		*(u32*)&_pBuffer[0] = SI_DANCEMAT;
	}
	else
	{
		return CSIDevice_GCController::RunBuffer(_pBuffer, _iLength);
	}

	return _iLength;
}

u32 CSIDevice_DanceMat::MapPadStatus(const GCPadStatus& pad_status)
{
	// Map the dpad to the blue arrows, the buttons to the orange arrows
	// Z = + button, Start = - button
	u16 map = 0;
	if (pad_status.button & PAD_BUTTON_UP)
		map |= 0x1000;
	if (pad_status.button & PAD_BUTTON_DOWN)
		map |= 0x2;
	if (pad_status.button & PAD_BUTTON_LEFT)
		map |= 0x8;
	if (pad_status.button & PAD_BUTTON_RIGHT)
		map |= 0x4;
	if (pad_status.button & PAD_BUTTON_Y)
		map |= 0x200;
	if (pad_status.button & PAD_BUTTON_A)
		map |= 0x10;
	if (pad_status.button & PAD_BUTTON_B)
		map |= 0x100;
	if (pad_status.button & PAD_BUTTON_X)
		map |= 0x800;
	if (pad_status.button & PAD_TRIGGER_Z)
		map |= 0x400;
	if (pad_status.button & PAD_BUTTON_START)
		map |= 0x1;

	return (u32)(map << 16) | 0x8080;
}

bool CSIDevice_DanceMat::GetData(u32& _Hi, u32& _Low)
{
	CSIDevice_GCController::GetData(_Hi, _Low);

	// Identifies the dance mat
	_Low = 0x8080ffff;

	return true;
}
