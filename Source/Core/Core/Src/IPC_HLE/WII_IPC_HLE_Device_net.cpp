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

#include "WII_IPC_HLE_Device_net.h"

CWII_IPC_HLE_Device_net_kd_request::CWII_IPC_HLE_Device_net_kd_request(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
}

CWII_IPC_HLE_Device_net_kd_request::~CWII_IPC_HLE_Device_net_kd_request()
{

}

bool CWII_IPC_HLE_Device_net_kd_request::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	return true;
}

bool CWII_IPC_HLE_Device_net_kd_request::IOCtl(u32 _CommandAddress) 
{
	LOG(WII_IPC_NET, "NET_KD_REQ: IOCtl (Device=%s)", GetDeviceName().c_str());	

	u32 Parameter =  Memory::Read_U32(_CommandAddress + 0xC);
	u32 BufferIn =  Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize =  Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

	u32 ReturnValue = ExecuteCommand(Parameter, BufferIn, BufferInSize, BufferOut, BufferOutSize);	
	Memory::Write_U32(ReturnValue, _CommandAddress+4);

	return true; 
}

s32 CWII_IPC_HLE_Device_net_kd_request::ExecuteCommand(u32 _Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize)
{
	switch(_Parameter)
	{
	default:
		LOG(WII_IPC_NET, "CWII_IPC_HLE_Device_net_kd_request::IOCtl: ni  0x%x (iBufferSize: %i, oBufferSize: %i)", _Parameter, _BufferInSize, _BufferOutSize);
		_dbg_assert_msg_(WII_IPC_NET, 0, "CWII_IPC_HLE_Device_net_kd_request::IOCtl: ni  0x%x (iBufferSize: %i, oBufferSize: %i)", _Parameter, _BufferInSize, _BufferOutSize);
		break;
	}
	return 0;
}

// **********************************************************************************

CWII_IPC_HLE_Device_net_ncd_manage::CWII_IPC_HLE_Device_net_ncd_manage(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{}

CWII_IPC_HLE_Device_net_ncd_manage::~CWII_IPC_HLE_Device_net_ncd_manage() 
{}

bool CWII_IPC_HLE_Device_net_ncd_manage::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	return true;
}


bool CWII_IPC_HLE_Device_net_ncd_manage::IOCtlV(u32 _CommandAddress) 
{ 
	u32 ReturnValue = 0;

	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	switch(CommandBuffer.Parameter)
	{
	default:
		LOG(WII_IPC_NET, "CWII_IPC_HLE_Device_fs::IOCtlV: %i", CommandBuffer.Parameter);
		_dbg_assert_msg_(WII_IPC_NET, 0, "CWII_IPC_HLE_Device_fs::IOCtlV: %i", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress+4);

	return true; 
}