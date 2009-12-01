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

#ifndef _WII_IPC_HLE_DEVICE_FILEIO_H_
#define _WII_IPC_HLE_DEVICE_FILEIO_H_

#include "WII_IPC_HLE_Device.h"

class CWII_IPC_HLE_Device_FileIO : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_FileIO(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_FileIO();

	bool Close(u32 _CommandAddress, bool _bForce);
    bool Open(u32 _CommandAddress, u32 _Mode);
	bool Seek(u32 _CommandAddress);
	bool Read(u32 _CommandAddress);
	bool Write(u32 _CommandAddress);
    bool IOCtl(u32 _CommandAddress);
	bool ReturnFileHandle();

private:

    enum
    {
        ISFS_FUNCNULL				= 0,
        ISFS_FUNCGETSTAT			= 1,
        ISFS_FUNCREADDIR			= 2,
        ISFS_FUNCGETATTR			= 3,
        ISFS_FUNCGETUSAGE			= 4,
    };

    enum
    {
        ISFS_IOCTL_FORMAT			= 1,
        ISFS_IOCTL_GETSTATS			= 2,
        ISFS_IOCTL_CREATEDIR		= 3,
        ISFS_IOCTL_READDIR			= 4,
        ISFS_IOCTL_SETATTR			= 5,
        ISFS_IOCTL_GETATTR			= 6,
        ISFS_IOCTL_DELETE			= 7,
        ISFS_IOCTL_RENAME			= 8,
        ISFS_IOCTL_CREATEFILE		= 9,
        ISFS_IOCTL_SETFILEVERCTRL	= 10,
        ISFS_IOCTL_GETFILESTATS		= 11,
        ISFS_IOCTL_GETUSAGE			= 12,
        ISFS_IOCTL_SHUTDOWN			= 13,
    };

    FILE* m_pFileHandle;
    u64 m_FileLength;

	std::string m_Filename;
};

#endif

