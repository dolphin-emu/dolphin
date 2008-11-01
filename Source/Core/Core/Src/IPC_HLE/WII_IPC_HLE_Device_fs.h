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
#ifndef _WII_IPC_HLE_DEVICE_FS_H_
#define _WII_IPC_HLE_DEVICE_FS_H_

#include "WII_IPC_HLE_Device.h"

class CWII_IPC_HLE_Device_fs : public IWII_IPC_HLE_Device
{
public:

    CWII_IPC_HLE_Device_fs(u32 _DeviceID, const std::string& _rDeviceName);
	virtual ~CWII_IPC_HLE_Device_fs();

    virtual bool Open(u32 _CommandAddress, u32 _Mode);

#if 0
    virtual bool Close(u32 _CommandAddress)  { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Close()", m_Name.c_str()); return true; }
    virtual bool Seek(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Seek()", m_Name.c_str()); return true; }
	virtual bool Read(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Read()", m_Name.c_str()); return true; }
	virtual bool Write(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Write()", m_Name.c_str()); return true; }
#endif

	virtual bool IOCtl(u32 _CommandAddress);

	virtual bool IOCtlV(u32 _CommandAddress);

private:

	enum 
	{
		CREATE_DIR			= 0x03,
		IOCTL_READ_DIR		= 0x04,
		GET_ATTR			= 0x06,
		DELETE_FILE			= 0x07,
		RENAME_FILE			= 0x08,
		CREATE_FILE			= 0x09,
		IOCTL_GETUSAGE		= 0x0C
	};

	s32 ExecuteCommand(u32 Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize);

	void CreateDirectoryStruct(const std::string& _rFullPath);

};

#endif

