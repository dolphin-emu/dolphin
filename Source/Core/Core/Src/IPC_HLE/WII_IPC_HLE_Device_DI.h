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
#ifndef _WII_IPC_HLE_DEVICE_DI_H_
#define _WII_IPC_HLE_DEVICE_DI_H_

#include "WII_IPC_HLE_Device.h"

namespace DiscIO
{
    class IVolume;
    class IFileSystem;
}

class CWII_IPC_HLE_Device_di : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_di(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_di();

    bool Open(u32 _CommandAddress, u32 _Mode);
	bool Close(u32 _CommandAddress);

	bool IOCtl(u32 _CommandAddress); 
	bool IOCtlV(u32 _CommandAddress);

private:

	u32 ExecuteCommand(u32 BufferIn, u32 BufferInSize, u32 _BufferOut, u32 BufferOutSize);

    DiscIO::IVolume* m_pVolume;
    DiscIO::IFileSystem* m_pFileSystem;
};

#endif
