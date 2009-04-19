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

// PRELIMINARY - doesn't work yet

#ifndef _WII_IPC_HLE_DEVICE_SDIO_SLOT0_H_
#define _WII_IPC_HLE_DEVICE_SDIO_SLOT0_H_

#include "WII_IPC_HLE_Device.h"

class CWII_IPC_HLE_Device_sdio_slot0 : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_sdio_slot0(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_sdio_slot0();

    bool Open(u32 _CommandAddress, u32 _Mode);
    bool Close(u32 _CommandAddress);
	bool IOCtl(u32 _CommandAddress); 
	bool IOCtlV(u32 _CommandAddress);

private:

	enum
	{
		SDIO_BASE = 0x8d070000,
	};

	enum 
	{
		IOCTL_WRITEHCREG	= 0x01,
		IOCTL_READHCREG		= 0x02,
		IOCTL_READCREG		= 0x03,
		IOCTL_RESETCARD		= 0x04,
		IOCTL_WRITECREG		= 0x05,
		IOCTL_SETCLK		= 0x06,
		IOCTL_SENDCMD		= 0x07,
		IOCTL_SETBUSWIDTH	= 0x08,
		IOCTL_READMCREG		= 0x09,
		IOCTL_WRITEMCREG	= 0x0A,
		IOCTL_GETSTATUS		= 0x0B,
		IOCTL_GETOCR		= 0x0C,
		IOCTL_READDATA		= 0x0D,
		IOCTL_WRITEDATA		= 0x0E,
	};

	enum
	{
		CARD_INSERTED		= 1,
		CARD_INITIALIZED	= 0x10000,
	};

	enum
	{
		SWITCH_FUNC			= 6,
		SELECT_CARD			= 7, // Selects or Deselects single card
		SEND_CID			= 10,
		
		SWITCH34			= 34,
		SWITCH35			= 35,
		SWITCH36			= 36,
		SWITCH37			= 37,
		SWITCH50			= 50,
		SWITCH57			= 57,
	};

	u32 m_status;

	u32 ExecuteCommand(u32 BufferIn, u32 BufferInSize, u32 _BufferOut, u32 BufferOutSize);
};

#endif
