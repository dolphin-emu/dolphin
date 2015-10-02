// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI_Device.h"

class PointerWrap;
struct KeyboardStatus;

class CSIDevice_Keyboard : public ISIDevice
{
protected:

	// Commands
	enum EBufferCommands
	{
		CMD_RESET       = 0x00,
		CMD_DIRECT      = 0x54,
		CMD_ID          = 0xff,
	};

	enum EDirectCommands
	{
		CMD_WRITE = 0x40,
		CMD_POLL  = 0x54
	};

	union UCommand
	{
		u32 Hex = 0;
		struct
		{
			u32 Parameter1 : 8;
			u32 Parameter2 : 8;
			u32 Command    : 8;
			u32            : 8;
		};

		constexpr UCommand() = default;
		constexpr UCommand(u32 hex) : Hex(hex) {}
	};

	// PADAnalogMode
	u8 m_Mode;

	// Internal counter synchonizing GC and keyboard
	u8 m_Counter;

public:

	// Constructor
	CSIDevice_Keyboard(SIDevices device, int _iDeviceNumber);

	// Run the SI Buffer
	int RunBuffer(u8* _pBuffer, int _iLength) override;

	// Return true on new data
	bool GetData(u32& _Hi, u32& _Low) override;

	KeyboardStatus GetKeyboardStatus();
	void MapKeys(KeyboardStatus& KeyStatus, u8* key);

	// Send a command directly
	void SendCommand(u32 _Cmd, u8 _Poll) override;

	// Savestate support
	void DoState(PointerWrap& p) override;
};
