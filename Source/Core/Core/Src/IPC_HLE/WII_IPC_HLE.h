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

#ifndef _WII_IPC_HLE_H_
#define _WII_IPC_HLE_H_

#include "ChunkFile.h"

class IWII_IPC_HLE_Device;

namespace WII_IPC_HLE_Interface
{

#define IPC_FIRST_HARDWARE_ID	0x13370000		// first IPC device ID
#define IPC_FIRST_FILEIO_ID		0x13380000		// first IPC file ID 

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

u32 GetDeviceIDByName(const std::string& _rDeviceName);

IWII_IPC_HLE_Device* AccessDeviceByID(u32 _ID);

void DeleteDeviceByID(u32 _ID);

void CopySettingsFile(std::string& DeviceName);

IWII_IPC_HLE_Device* CreateFileIO(u32 _DeviceID, const std::string& _rDeviceName);

// Update
void Update();

// Update Devices
void UpdateDevices();

void ExecuteCommand(u32 _Address);

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

