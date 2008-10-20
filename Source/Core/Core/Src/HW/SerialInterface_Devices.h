// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _SERIALINTERFACE_DEVICES_H
#define _SERIALINTERFACE_DEVICES_H

#include "Common.h"
class PointerWrap;

class ISIDevice
{
protected:
	int m_iDeviceNumber;

public:

	// constructor
	ISIDevice(int _iDeviceNumber) : 
		m_iDeviceNumber(_iDeviceNumber)
	{ }
	virtual ~ISIDevice() { }

	// run the SI Buffer
	virtual int RunBuffer(u8* _pBuffer, int _iLength);

	// return true on new data
	virtual bool GetData(u32& _Hi, u32& _Low) = 0;

	// send a command directly (no detour per buffer)
	virtual void SendCommand(u32 _Cmd) = 0;
};

// =====================================================================================================
// standard gamecube controller
// =====================================================================================================

class CSIDevice_GCController : public ISIDevice
{
private:
	
	// commands
	enum EBufferCommands
	{
		CMD_INVALID		= 0xFFFFFFFF,
		CMD_RESET		= 0x00,
		CMD_ORIGIN		= 0x41,
		CMD_RECALIBRATE	= 0x42,
	};

	struct SOrigin
	{
		u8 uCommand;
		u8 unk_1;
		u8 uOriginStickX;
		u8 uOriginStickY;
		u8 uSubStickStickX; // ???
		u8 uSubStickStickY; // ???
		u8 uTrigger_L;		 // ???
		u8 uTrigger_R;		 // ???
		u8 unk_4;
		u8 unk_5;
		u8 unk_6;
		u8 unk_7;
	};

	enum EDirectCommands
	{
		CMD_RUMBLE = 0x40
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
		UCommand()			{Hex = 0;}
		UCommand(u32 _iValue)	{Hex = _iValue;}		
	};

	SOrigin m_origin;
	int DeviceNum;

public:

	// constructor
	CSIDevice_GCController(int _iDeviceNumber);

	// run the SI Buffer
	virtual int RunBuffer(u8* _pBuffer, int _iLength);

	// return true on new data
	virtual bool GetData(u32& _Hi, u32& _Low);

	// send a command directly
	virtual void SendCommand(u32 _Cmd);
};

// =====================================================================================================
// dummy - no device attached
// =====================================================================================================

class CSIDevice_Dummy : public ISIDevice
{
public:

	// constructor
	CSIDevice_Dummy(int _iDeviceNumber);

	// run the SI Buffer
	virtual int RunBuffer(u8* _pBuffer, int _iLength);

	// return true on new data
	virtual bool GetData(u32& _Hi, u32& _Low);

	// send a command directly
	virtual void SendCommand(u32 _Cmd);
};

#endif

