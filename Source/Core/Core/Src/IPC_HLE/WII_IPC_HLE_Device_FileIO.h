// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _WII_IPC_HLE_DEVICE_FILEIO_H_
#define _WII_IPC_HLE_DEVICE_FILEIO_H_

#include "WII_IPC_HLE_Device.h"
#include "FileUtil.h"

std::string HLE_IPC_BuildFilename(std::string _pFilename, int _size);

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
	void DoState(PointerWrap &p);

	File::IOFile OpenFile();

private:
	enum
	{
		ISFS_OPEN_READ				= 1,
		ISFS_OPEN_WRITE,
		ISFS_OPEN_RW				= (ISFS_OPEN_READ | ISFS_OPEN_WRITE)
	};

	enum
	{
		ISFS_FUNCNULL				= 0,
		ISFS_FUNCGETSTAT,
		ISFS_FUNCREADDIR,
		ISFS_FUNCGETATTR,
		ISFS_FUNCGETUSAGE
	};

	enum
	{
		ISFS_IOCTL_FORMAT			= 1,
		ISFS_IOCTL_GETSTATS,
		ISFS_IOCTL_CREATEDIR,
		ISFS_IOCTL_READDIR,
		ISFS_IOCTL_SETATTR,
		ISFS_IOCTL_GETATTR,
		ISFS_IOCTL_DELETE,
		ISFS_IOCTL_RENAME,
		ISFS_IOCTL_CREATEFILE,
		ISFS_IOCTL_SETFILEVERCTRL,
		ISFS_IOCTL_GETFILESTATS,
		ISFS_IOCTL_GETUSAGE,
		ISFS_IOCTL_SHUTDOWN
	};

	u32 m_Mode;
	u32 m_SeekPos;
	
	std::string m_filepath;
};

#endif

