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

#ifndef _WII_IPC_HLE_DEVICE_ERROR_H_
#define _WII_IPC_HLE_DEVICE_ERROR_H_

#include "WII_IPC_HLE_Device.h"

class CWII_IPC_HLE_Device_Error : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_Error(u32 _DeviceID, const std::string& _rDeviceName ) :
		IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
	{}

	virtual ~CWII_IPC_HLE_Device_Error()
	{}

    virtual bool Open(u32 _CommandAddress, u32 _Mode)
    {
		PanicAlert("CWII_IPC_HLE_Device_Error");
        Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
        return true;
    }
};

#endif

