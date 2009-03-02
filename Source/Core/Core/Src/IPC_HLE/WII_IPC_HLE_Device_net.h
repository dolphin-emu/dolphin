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

#ifndef _WII_IPC_HLE_DEVICE_NET_H_
#define _WII_IPC_HLE_DEVICE_NET_H_

#include "WII_IPC_HLE_Device.h"

class CWII_IPC_HLE_Device_net_kd_request : public IWII_IPC_HLE_Device
{
public:

    CWII_IPC_HLE_Device_net_kd_request(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_net_kd_request();

	virtual bool Open(u32 _CommandAddress, u32 _Mode);
	virtual bool Close(u32 _CommandAddress);
	virtual bool IOCtl(u32 _CommandAddress);

private:

	s32 ExecuteCommand(u32 _Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize);
};

// **************************************************************************************

class CWII_IPC_HLE_Device_net_kd_time : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_net_kd_time(u32 _DeviceID, const std::string& _rDeviceName) :
		IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
		{}

	virtual ~CWII_IPC_HLE_Device_net_kd_time()
	{}

	virtual bool Open(u32 _CommandAddress, u32 _Mode)
	{
		INFO_LOG(WII_IPC_NET, "%s - IOCtl: Open",  GetDeviceName().c_str());
		Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
		return true;
	}

	virtual bool Close(u32 _CommandAddress, u32 _Mode)
	{
		INFO_LOG(WII_IPC_NET, "%s - IOCtl: Close",  GetDeviceName().c_str());
		Memory::Write_U32(0, _CommandAddress + 4);
		return true;
	}

	virtual bool IOCtl(u32 _CommandAddress) 
	{
		#ifdef LOGGING
		u32 Parameter = Memory::Read_U32(_CommandAddress +0x0C);
		u32 Buffer1 = Memory::Read_U32(_CommandAddress +0x10);
		u32 BufferSize1 = Memory::Read_U32(_CommandAddress +0x14);
		u32 Buffer2 = Memory::Read_U32(_CommandAddress +0x18);
		u32 BufferSize2 = Memory::Read_U32(_CommandAddress +0x1C);
		#endif

		// write return value
		Memory::Write_U32(0, _CommandAddress + 0x4);

		INFO_LOG(WII_IPC_NET, "%s - IOCtl:\n"
			"    Parameter: 0x%x   (0x17 could be some kind of Sync RTC) \n"
			"    Buffer1: 0x%08x\n"
			"    BufferSize1: 0x%08x\n"
			"    Buffer2: 0x%08x\n"
			"    BufferSize2: 0x%08x\n",
		GetDeviceName().c_str(), Parameter, Buffer1, BufferSize1, Buffer2, BufferSize2);

		return true;
	}
};

// **************************************************************************************

class CWII_IPC_HLE_Device_net_ip_top : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_ip_top(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_net_ip_top();

	virtual bool Open(u32 _CommandAddress, u32 _Mode);
	virtual bool Close(u32 _CommandAddress);
	virtual bool IOCtl(u32 _CommandAddress);
	virtual bool IOCtlV(u32 _CommandAddress);
	
private:
	s32 ExecuteCommand(u32 _Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize);
};

// **************************************************************************************

class CWII_IPC_HLE_Device_net_ncd_manage : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_ncd_manage(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_net_ncd_manage();

	virtual bool Open(u32 _CommandAddress, u32 _Mode);
	virtual bool Close(u32 _CommandAddress);
	virtual bool IOCtlV(u32 _CommandAddress);

};

#endif
