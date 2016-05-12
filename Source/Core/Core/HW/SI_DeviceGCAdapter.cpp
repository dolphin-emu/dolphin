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

int CSIDevice_GCAdapter::RunBuffer(u8* _pBuffer, int _iLength)
{
	// For debug logging only
	ISIDevice::RunBuffer(_pBuffer, _iLength);

	// Read the command
	EBufferCommands command = static_cast<EBufferCommands>(_pBuffer[3]);

	const u8 numPAD = NetPlay_InGamePadToLocalPad(ISIDevice::m_iDeviceNumber);
	if (numPAD < 4)
	{
		if (!GCAdapter::DeviceConnected(numPAD))
		{
			reinterpret_cast<u32*>(_pBuffer)[0] = SI_NONE;
			return 4;
		}
	}

	// Handle it
	switch (command)
	{
	case CMD_RESET:
	case CMD_ID:
		*(u32*)&_pBuffer[0] = SI_GC_CONTROLLER;
		break;

	case CMD_DIRECT:
		{
			INFO_LOG(SERIALINTERFACE, "PAD - Direct (Length: %d)", _iLength);
			u32 high, low;
			GetData(high, low);
			for (int i = 0; i < (_iLength - 1) / 2; i++)
			{
				_pBuffer[i + 0] = (high >> (i * 8)) & 0xff;
				_pBuffer[i + 4] = (low >> (i * 8)) & 0xff;
			}
		}
		break;

	case CMD_ORIGIN:
		{
			INFO_LOG(SERIALINTERFACE, "PAD - Get Origin");

			Calibrate();

			u8* pCalibration = reinterpret_cast<u8*>(&m_Origin);
			for (int i = 0; i < (int)sizeof(SOrigin); i++)
			{
				_pBuffer[i ^ 3] = *pCalibration++;
			}
		}
		break;

	// Recalibrate (FiRES: i am not 100 percent sure about this)
	case CMD_RECALIBRATE:
		{
			INFO_LOG(SERIALINTERFACE, "PAD - Recalibrate");

			Calibrate();

			u8* pCalibration = reinterpret_cast<u8*>(&m_Origin);
			for (int i = 0; i < (int)sizeof(SOrigin); i++)
			{
				_pBuffer[i ^ 3] = *pCalibration++;
			}
		}
		break;

	// DEFAULT
	default:
		{
			ERROR_LOG(SERIALINTERFACE, "Unknown SI command     (0x%x)", command);
			PanicAlert("SI: Unknown command (0x%x)", command);
		}
		break;
	}

	return _iLength;
}

void CSIDevice_GCController::Rumble(u8 numPad, ControlState strength)
{
	SIDevices device = SConfig::GetInstance().m_SIDevice[numPad];
	if (device == SIDEVICE_WIIU_ADAPTER)
		GCAdapter::Output(numPad, static_cast<u8>(strength));
	else if (SIDevice_IsGCController(device))
		Pad::Rumble(numPad, strength);
}
