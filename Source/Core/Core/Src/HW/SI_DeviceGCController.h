// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _SI_DEVICEGCCONTROLLER_H
#define _SI_DEVICEGCCONTROLLER_H

#include "../PluginManager.h"
#include "SI_Device.h"


// standard gamecube controller
class CSIDevice_GCController : public ISIDevice
{
private:
	
	// Commands
	enum EBufferCommands
	{
		CMD_INVALID		= 0xFFFFFFFF,
		CMD_RESET		= 0x00,
		CMD_ORIGIN		= 0x41,
		CMD_RECALIBRATE	= 0x42,
	};

	struct SOrigin
	{
		u8 uCommand;// Maybe should be button bits?
		u8 unk_1;	// ..and this would be the other half
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
			unsigned Parameter1	:	8;
			unsigned Parameter2	:	8;
			unsigned Command	:	8;
			unsigned			:	8;
		};
		UCommand()				{Hex = 0;}
		UCommand(u32 _iValue)	{Hex = _iValue;}		
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

	// PADAnalogMode
	u8 m_Mode;

	// Timer to track special button combos:
	// y, X, start for 3 seconds updates origin with current status
	//   Technically, the above is only on standard pad, wavebird does not support it for example
	// b, x, start for 3 seconds triggers reset (PI reset button interrupt)
	u32 m_TButtonComboStart, m_TButtonCombo;
	// Type of button combo from the last/current poll
	EButtonCombo m_LastButtonCombo;

public:

	// Constructor
	CSIDevice_GCController(int _iDeviceNumber);

	// Run the SI Buffer
	virtual int RunBuffer(u8* _pBuffer, int _iLength);

	// Send and Receive pad input from network
	static int GetNetInput(u8 numPAD, SPADStatus, u32 *PADStatus);

	// Return true on new data
	virtual bool GetData(u32& _Hi, u32& _Low);

	// Send a command directly
	virtual void SendCommand(u32 _Cmd, u8 _Poll);
};
#endif
