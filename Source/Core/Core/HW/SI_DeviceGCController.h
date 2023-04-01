// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/GCPad.h"
#include "Core/HW/SI_Device.h"
#include "InputCommon/GCPadStatus.h"

class CSIDevice_GCController : public ISIDevice
{
protected:

	// Commands
	enum EBufferCommands
	{
		CMD_RESET       = 0x00,
		CMD_DIRECT      = 0x40,
		CMD_ORIGIN      = 0x41,
		CMD_RECALIBRATE = 0x42,
		CMD_ID          = 0xff,
	};

	struct SOrigin
	{
		u16 uButton;
		u8 uOriginStickX;
		u8 uOriginStickY;
		u8 uSubStickStickX;
		u8 uSubStickStickY;
		u8 uTrigger_L;
		u8 uTrigger_R;
		u8 unk_4;
		u8 unk_5;
		u8 unk_6;
		u8 unk_7;
	};

	enum EDirectCommands
	{
		CMD_WRITE = 0x40
	};

	union UCommand
	{
		u32 Hex;
		struct
		{
			u32 Parameter1 : 8;
			u32 Parameter2 : 8;
			u32 Command    : 8;
			u32            : 8;
		};
		UCommand()            {Hex = 0;}
		UCommand(u32 _iValue) {Hex = _iValue;}
	};

	enum EButtonCombo
	{
		COMBO_NONE = 0,
		COMBO_ORIGIN,
		COMBO_RESET
	};

	// struct to compare input against
	// Set on connection and (standard pad only) on button combo
	SOrigin m_Origin;

	bool m_Calibrated;

	// PADAnalogMode
	u8 m_Mode;

	// Timer to track special button combos:
	// y, X, start for 3 seconds updates origin with current status
	//   Technically, the above is only on standard pad, wavebird does not support it for example
	// b, x, start for 3 seconds triggers reset (PI reset button interrupt)
	u64 m_TButtonComboStart, m_TButtonCombo;
	// Type of button combo from the last/current poll
	EButtonCombo m_LastButtonCombo;

	// Set this if we want to simulate the "TaruKonga" DK Bongo controller
	bool m_simulate_konga = false;

public:

	// Constructor
	CSIDevice_GCController(SIDevices device, int _iDeviceNumber);

	// Run the SI Buffer
	int RunBuffer(u8* _pBuffer, int _iLength) override;

	// Return true on new data
	bool GetData(u32& _Hi, u32& _Low) override;

	// Send a command directly
	void SendCommand(u32 _Cmd, u8 _Poll) override;

	// Savestate support
	void DoState(PointerWrap& p) override;

	virtual GCPadStatus GetPadStatus();
	virtual u32 MapPadStatus(const GCPadStatus& pad_status);
	virtual EButtonCombo HandleButtonCombos(const GCPadStatus& pad_status);

	// Send and Receive pad input from network
	static bool NetPlay_GetInput(u8 numPAD, GCPadStatus* status);
	static u8 NetPlay_InGamePadToLocalPad(u8 numPAD);

	// Direct rumble to the right GC Controller
	static void Rumble(u8 numPad, ControlState strength);

protected:
	void Calibrate();
	void HandleMoviePadStatus(GCPadStatus* PadStatus);
};


// "TaruKonga", the DK Bongo controller
class CSIDevice_TaruKonga : public CSIDevice_GCController
{
public:
	CSIDevice_TaruKonga(SIDevices device, int _iDeviceNumber) : CSIDevice_GCController(device, _iDeviceNumber)
	{
		m_simulate_konga = true;
	}
};
