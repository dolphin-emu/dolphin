// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _SIDEVICE_H
#define _SIDEVICE_H

#include "Common.h"

// Devices can reply with these, but idk if we'll ever use them...
#define SI_ERROR_NO_RESPONSE    0x0008 // Nothing is attached
#define SI_ERROR_UNKNOWN        0x0040 // Unknown device is attached
#define SI_ERROR_BUSY           0x0080 // Still detecting

// Device types
#define SI_TYPE_MASK            0x18000000u // ???
#define SI_TYPE_GC              0x08000000u

// GC Controller types
#define SI_GC_NOMOTOR           0x20000000u // No rumble motor
#define SI_GC_STANDARD          0x01000000u

class ISIDevice
{
protected:
	int m_iDeviceNumber;

public:
	// Constructor
	ISIDevice(int _iDeviceNumber) :
	  m_iDeviceNumber(_iDeviceNumber)
	{}

	// Destructor
	virtual ~ISIDevice() {}

	// Run the SI Buffer
	virtual int RunBuffer(u8* _pBuffer, int _iLength);

	// Return true on new data
	virtual bool GetData(u32& _Hi, u32& _Low) = 0;

	// Send a command directly (no detour per buffer)
	virtual void SendCommand(u32 _Cmd, u8 _Poll) = 0;
};

// SI Device IDs
enum TSIDevices
{
	SI_DUMMY			= 0,
	SI_N64_MIC			= 0x00010000,
	SI_N64_KEYBOARD		= 0x00020000,
	SI_N64_MOUSE		= 0x02000000,
	SI_N64_CONTROLLER	= 0x05000000,
	SI_GBA				= 0x00040000,
	SI_GC_CONTROLLER	= (SI_TYPE_GC | SI_GC_STANDARD),
	SI_GC_KEYBOARD		= (SI_TYPE_GC | 0x00200000),
	SI_GC_STEERING		= SI_TYPE_GC, // (shuffle2)I think the "chainsaw" is the same (Or else it's just standard)
};

extern ISIDevice* SIDevice_Create(TSIDevices _SIDevice, int _iDeviceNumber);

#endif
