// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _WII_IPC_HLE_DEVICE_FS_H_
#define _WII_IPC_HLE_DEVICE_FS_H_

#include "WII_IPC_HLE_Device.h"

struct NANDStat
{
	u32 BlockSize;
	u32 FreeUserBlocks;
	u32 UsedUserBlocks;
	u32 FreeSysBlocks;
	u32 UsedSysBlocks;
	u32 Free_INodes;
	u32 Used_Inodes;
};

enum {
	FS_RESULT_OK			=  0,
	FS_INVALID				= -4,
	FS_DIRFILE_NOT_FOUND	= -6,
	FS_RESULT_FATAL			= -101,
	FS_NO_ACCESS			= -102,
	FS_FILE_EXIST			= -105,
	FS_FILE_NOT_EXIST		= -106,
	FS_NO_HANDLE			= -106,
};

class CWII_IPC_HLE_Device_fs : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_fs(u32 _DeviceID, const std::string& _rDeviceName);
	virtual ~CWII_IPC_HLE_Device_fs();

	virtual void DoState(PointerWrap& p);

	virtual bool Open(u32 _CommandAddress, u32 _Mode);
	virtual bool Close(u32 _CommandAddress, bool _bForce);

	virtual bool IOCtl(u32 _CommandAddress);
	virtual bool IOCtlV(u32 _CommandAddress);
	
	virtual int GetCmdDelay(u32);

private:

	enum
	{
		IOCTL_GET_STATS		= 0x02,
		IOCTL_CREATE_DIR	= 0x03,
		IOCTLV_READ_DIR		= 0x04,
		IOCTL_SET_ATTR		= 0x05,
		IOCTL_GET_ATTR		= 0x06,
		IOCTL_DELETE_FILE	= 0x07,
		IOCTL_RENAME_FILE	= 0x08,
		IOCTL_CREATE_FILE	= 0x09,
		IOCTLV_GETUSAGE		= 0x0C
	};

	s32 ExecuteCommand(u32 Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize);
};

#endif

