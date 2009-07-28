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

#ifndef _EXIDEVICE_H
#define _EXIDEVICE_H

#include "Common.h"

class IEXIDevice
{
private:
	// Byte transfer function for this device
	virtual void TransferByte(u8&) {};

public:
	// Immediate copy functions
	virtual void ImmWrite(u32 _uData,  u32 _uSize);
	virtual u32 ImmRead(u32 _uSize);

	// DMA copy functions
	virtual void DMAWrite(u32 _uAddr, u32 _uSize);
	virtual void DMARead (u32 _uAddr, u32 _uSize);

	virtual bool IsPresent();
	virtual void SetCS(int _iCS);

	// Update
	virtual void Update();

	// Is generating interrupt ?
	virtual bool IsInterruptSet();

	virtual ~IEXIDevice() {};
};

enum TEXIDevices
{
	EXIDEVICE_DUMMY,
	EXIDEVICE_MEMORYCARD_A,
	EXIDEVICE_MEMORYCARD_B,
	EXIDEVICE_IPL,
	EXIDEVICE_AD16,
	EXIDEVICE_MIC,
	EXIDEVICE_ETH,
};

extern IEXIDevice* EXIDevice_Create(TEXIDevices _EXIDevice);

#endif

