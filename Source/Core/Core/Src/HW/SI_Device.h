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

#ifndef _SIDEVICE_H
#define _SIDEVICE_H

#include "Common.h"

#define SI_ERROR_NO_RESPONSE    0x0008 // nothing is attached
#define SI_ERROR_UNKNOWN        0x0040 // unknown device is attached
#define SI_ERROR_BUSY           0x0080 // still detecting

// Device types
#define SI_TYPE_MASK            0x18000000u
#define SI_TYPE_N64             0x00000000u
#define SI_TYPE_GC              0x08000000u

// GameCube
#define SI_GC_WIRELESS          0x80000000u
#define SI_GC_NOMOTOR           0x20000000u // no rumble motor
#define SI_GC_STANDARD          0x01000000u

// WaveBird
#define SI_WIRELESS_RECEIVED    0x40000000u // 0: no wireless unit
#define SI_WIRELESS_IR          0x04000000u // 0: IR  1: RF
#define SI_WIRELESS_STATE       0x02000000u // 0: variable  1: fixed
#define SI_WIRELESS_ORIGIN      0x00200000u // 0: invalid  1: valid
#define SI_WIRELESS_FIX_ID      0x00100000u // 0: not fixed  1: fixed
#define SI_WIRELESS_TYPE        0x000f0000u
#define SI_WIRELESS_LITE_MASK   0x000c0000u // 0: normal 1: lite controller
#define SI_WIRELESS_LITE        0x00040000u // 0: normal 1: lite controller
#define SI_WIRELESS_CONT_MASK   0x00080000u // 0: non-controller 1: non-controller
#define SI_WIRELESS_CONT        0x00000000u
#define SI_WIRELESS_ID          0x00c0ff00u
#define SI_WIRELESS_TYPE_ID     (SI_WIRELESS_TYPE | SI_WIRELESS_ID)

// "Complete" IDs
#define SI_N64_CONTROLLER       (SI_TYPE_N64 | 0x05000000)
#define SI_N64_MIC              (SI_TYPE_N64 | 0x00010000)
#define SI_N64_KEYBOARD         (SI_TYPE_N64 | 0x00020000)
#define SI_N64_MOUSE            (SI_TYPE_N64 | 0x02000000)
#define SI_GBA                  (SI_TYPE_N64 | 0x00040000)
#define SI_GC_CONTROLLER        (SI_TYPE_GC | SI_GC_STANDARD)
#define SI_GC_RECEIVER          (SI_TYPE_GC | SI_GC_WIRELESS)
#define SI_GC_WAVEBIRD          (SI_TYPE_GC | SI_GC_WIRELESS | SI_GC_STANDARD | SI_WIRELESS_STATE | SI_WIRELESS_FIX_ID)
#define SI_GC_KEYBOARD          (SI_TYPE_GC | 0x00200000)
#define SI_GC_STEERING          (SI_TYPE_GC | 0x00000000)


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
