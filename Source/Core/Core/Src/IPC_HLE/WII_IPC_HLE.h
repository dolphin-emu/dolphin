// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _WII_IPC_HLE_H_
#define _WII_IPC_HLE_H_

#include "ChunkFile.h"

class IWII_IPC_HLE_Device;

namespace WII_IPC_HLE_Interface
{

#define IPC_FIRST_ID	0x00		// first IPC device ID
#define IPC_MAX_FILES	0x10		// first IPC file ID 
	
// Init
void Init();

// Shutdown
void Shutdown();	

// Reset
void Reset(bool _bHard = false);

// Do State
void DoState(PointerWrap &p);

// Set default content file
void SetDefaultContentFile(const std::string& _rFilename);
void ES_DIVerify(u8 *_pTMD, u32 _sz);

void SDIO_EventNotify();


IWII_IPC_HLE_Device* CreateFileIO(u32 _DeviceID, const std::string& _rDeviceName);

IWII_IPC_HLE_Device* GetDeviceByName(const std::string& _rDeviceName);
IWII_IPC_HLE_Device* AccessDeviceByID(u32 _ID);
int getFreeDeviceId();

// Update
void Update();

// Update Devices
void UpdateDevices();

void ExecuteCommand(u32 _Address);

void EnqRequest(u32 _Address);
void EnqReply(u32 _Address, int cycles_in_future = 0);

enum ECommandType
{
	COMMAND_OPEN_DEVICE		= 1,
	COMMAND_CLOSE_DEVICE	= 2,
	COMMAND_READ			= 3,
	COMMAND_WRITE			= 4,
	COMMAND_SEEK			= 5,
	COMMAND_IOCTL			= 6,
	COMMAND_IOCTLV			= 7,
};

} // end of namespace WII_IPC_HLE_Interface

#endif

