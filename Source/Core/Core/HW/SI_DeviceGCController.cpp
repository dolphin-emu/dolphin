// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Movie.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceMic.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI.h"
#include "Core/HW/SI_Device.h"
#include "Core/HW/SI_DeviceGCController.h"
#if defined(__LIBUSB__) || defined (_WIN32)
#include "Core/HW/SI_GCAdapter.h"
#endif
#include "Core/HW/SystemTimers.h"
#include "InputCommon/GCPadStatus.h"

// --- standard GameCube controller ---
CSIDevice_GCController::CSIDevice_GCController(SIDevices device, int _iDeviceNumber)
	: ISIDevice(device, _iDeviceNumber)
	, m_TButtonComboStart(0)
	, m_TButtonCombo(0)
	, m_LastButtonCombo(COMBO_NONE)
{
	// Dunno if we need to do this, game/lib should set it?
	m_Mode                   = 0x03;

	m_Calibrated = false;
}

void CSIDevice_GCController::Calibrate()
{
	GCPadStatus pad_origin = GetPadStatus();
	memset(&m_Origin, 0, sizeof(SOrigin));
	m_Origin.uButton = pad_origin.button;
	m_Origin.uOriginStickX = pad_origin.stickX;
	m_Origin.uOriginStickY = pad_origin.stickY;
	m_Origin.uSubStickStickX = pad_origin.substickX;
	m_Origin.uSubStickStickY = pad_origin.substickY;
	m_Origin.uTrigger_L = pad_origin.triggerLeft;
	m_Origin.uTrigger_R = pad_origin.triggerRight;

	m_Calibrated = true;
}

int CSIDevice_GCController::RunBuffer(u8* _pBuffer, int _iLength)
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

			if (!m_Calibrated)
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

			if (!m_Calibrated)
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

GCPadStatus CSIDevice_GCController::GetPadStatus()
{
	GCPadStatus PadStatus;
	memset(&PadStatus, 0, sizeof(PadStatus));

	Pad::GetStatus(ISIDevice::m_iDeviceNumber, &PadStatus);

#if defined(__LIBUSB__) || defined (_WIN32)
	SI_GCAdapter::Input(ISIDevice::m_iDeviceNumber, &PadStatus);
#endif

	Movie::CallGCInputManip(&PadStatus, ISIDevice::m_iDeviceNumber);

	Movie::SetPolledDevice();
	if (NetPlay_GetInput(ISIDevice::m_iDeviceNumber, &PadStatus))
	{
	}
	else if (Movie::IsPlayingInput())
	{
		Movie::PlayController(&PadStatus, ISIDevice::m_iDeviceNumber);
		Movie::InputUpdate();
	}
	else if (Movie::IsRecordingInput())
	{
		Movie::RecordInput(&PadStatus, ISIDevice::m_iDeviceNumber);
		Movie::InputUpdate();
	}
	else
	{
		Movie::CheckPadStatus(&PadStatus, ISIDevice::m_iDeviceNumber);
	}

	return PadStatus;
}

// GetData

// Return true on new data (max 7 Bytes and 6 bits ;)
// [00?SYXBA] [1LRZUDRL] [x] [y] [cx] [cy] [l] [r]
//  |\_ ERR_LATCH (error latched - check SISR)
//  |_ ERR_STATUS (error on last GetData or SendCmd?)
bool CSIDevice_GCController::GetData(u32& _Hi, u32& _Low)
{
	GCPadStatus PadStatus = GetPadStatus();

	_Hi = MapPadStatus(PadStatus);

	// Low bits are packed differently per mode
	if (m_Mode == 0 || m_Mode == 5 || m_Mode == 6 || m_Mode == 7)
	{
		_Low  = (u8)(PadStatus.analogB >> 4);                   // Top 4 bits
		_Low |= (u32)((u8)(PadStatus.analogA >> 4) << 4);       // Top 4 bits
		_Low |= (u32)((u8)(PadStatus.triggerRight >> 4) << 8);  // Top 4 bits
		_Low |= (u32)((u8)(PadStatus.triggerLeft >> 4) << 12);  // Top 4 bits
		_Low |= (u32)((u8)(PadStatus.substickY) << 16);         // All 8 bits
		_Low |= (u32)((u8)(PadStatus.substickX) << 24);         // All 8 bits
	}
	else if (m_Mode == 1)
	{
		_Low  = (u8)(PadStatus.analogB >> 4);                   // Top 4 bits
		_Low |= (u32)((u8)(PadStatus.analogA >> 4) << 4);       // Top 4 bits
		_Low |= (u32)((u8)PadStatus.triggerRight << 8);         // All 8 bits
		_Low |= (u32)((u8)PadStatus.triggerLeft << 16);         // All 8 bits
		_Low |= (u32)((u8)PadStatus.substickY << 24);           // Top 4 bits
		_Low |= (u32)((u8)PadStatus.substickX << 28);           // Top 4 bits
	}
	else if (m_Mode == 2)
	{
		_Low  = (u8)(PadStatus.analogB);                        // All 8 bits
		_Low |= (u32)((u8)(PadStatus.analogA) << 8);            // All 8 bits
		_Low |= (u32)((u8)(PadStatus.triggerRight >> 4) << 16); // Top 4 bits
		_Low |= (u32)((u8)(PadStatus.triggerLeft >> 4) << 20);  // Top 4 bits
		_Low |= (u32)((u8)PadStatus.substickY << 24);           // Top 4 bits
		_Low |= (u32)((u8)PadStatus.substickX << 28);           // Top 4 bits
	}
	else if (m_Mode == 3)
	{
		// Analog A/B are always 0
		_Low  = (u8)PadStatus.triggerRight;                     // All 8 bits
		_Low |= (u32)((u8)PadStatus.triggerLeft << 8);          // All 8 bits
		_Low |= (u32)((u8)PadStatus.substickY << 16);           // All 8 bits
		_Low |= (u32)((u8)PadStatus.substickX << 24);           // All 8 bits
	}
	else if (m_Mode == 4)
	{
		_Low  = (u8)(PadStatus.analogB);                        // All 8 bits
		_Low |= (u32)((u8)(PadStatus.analogA) << 8);            // All 8 bits
		// triggerLeft/Right are always 0
		_Low |= (u32)((u8)PadStatus.substickY << 16);           // All 8 bits
		_Low |= (u32)((u8)PadStatus.substickX << 24);           // All 8 bits
	}

	HandleButtonCombos(PadStatus);
	return true;
}

u32 CSIDevice_GCController::MapPadStatus(const GCPadStatus& pad_status)
{
	// Thankfully changing mode does not change the high bits ;)
	u32 _Hi = 0;
	_Hi  = (u32)((u8)pad_status.stickY);
	_Hi |= (u32)((u8)pad_status.stickX << 8);
	_Hi |= (u32)((u16)(pad_status.button | PAD_USE_ORIGIN) << 16);
	return _Hi;
}

void CSIDevice_GCController::HandleButtonCombos(const GCPadStatus& pad_status)
{
	// Keep track of the special button combos (embedded in controller hardware... :( )
	EButtonCombo tempCombo;
	if ((pad_status.button & 0xff00) == (PAD_BUTTON_Y|PAD_BUTTON_X|PAD_BUTTON_START))
		tempCombo = COMBO_ORIGIN;
	else if ((pad_status.button & 0xff00) == (PAD_BUTTON_B|PAD_BUTTON_X|PAD_BUTTON_START))
		tempCombo = COMBO_RESET;
	else
		tempCombo = COMBO_NONE;
	if (tempCombo != m_LastButtonCombo)
	{
		m_LastButtonCombo = tempCombo;
		if (m_LastButtonCombo != COMBO_NONE)
			m_TButtonComboStart = CoreTiming::GetTicks();
	}
	if (m_LastButtonCombo != COMBO_NONE)
	{
		m_TButtonCombo = CoreTiming::GetTicks();
		if ((m_TButtonCombo - m_TButtonComboStart) > SystemTimers::GetTicksPerSecond() * 3)
		{
			if (m_LastButtonCombo == COMBO_RESET)
				ProcessorInterface::ResetButton_Tap();
			else if (m_LastButtonCombo == COMBO_ORIGIN)
			{
				m_Origin.uOriginStickX   = pad_status.stickX;
				m_Origin.uOriginStickY   = pad_status.stickY;
				m_Origin.uSubStickStickX = pad_status.substickX;
				m_Origin.uSubStickStickY = pad_status.substickY;
				m_Origin.uTrigger_L      = pad_status.triggerLeft;
				m_Origin.uTrigger_R      = pad_status.triggerRight;
			}
			m_LastButtonCombo = COMBO_NONE;
		}
	}
}

// SendCommand
void CSIDevice_GCController::SendCommand(u32 _Cmd, u8 _Poll)
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

#if defined(__LIBUSB__) || defined (_WIN32)
			if (numPAD < 4)
			{
				if (uType == 1 && uStrength > 2)
					SI_GCAdapter::Output(numPAD, 1);
				else
					SI_GCAdapter::Output(numPAD, 0);
			}
#endif
			if (numPAD < 4)
			{
				if (uType == 1 && uStrength > 2)
					Pad::Rumble(numPAD, 1.0);
				else
					Pad::Rumble(numPAD, 0.0);
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

// Savestate support
void CSIDevice_GCController::DoState(PointerWrap& p)
{
	p.Do(m_Calibrated);
	p.Do(m_Origin);
	p.Do(m_Mode);
	p.Do(m_TButtonComboStart);
	p.Do(m_TButtonCombo);
	p.Do(m_LastButtonCombo);
}
