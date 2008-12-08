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
#ifndef _WII_IPC_HLE_DEVICE_STM_H_
#define _WII_IPC_HLE_DEVICE_STM_H_

#include "WII_IPC_HLE_Device.h"

enum
{
    IOCTL_STM_EVENTHOOK			= 0x1000,
    IOCTL_STM_GET_IDLEMODE		= 0x3001,
    IOCTL_STM_RELEASE_EH		= 0x3002,
    IOCTL_STM_HOTRESET			= 0x2001,
    IOCTL_STM_HOTRESET_FOR_PD	= 0x2002,
    IOCTL_STM_SHUTDOWN			= 0x2003,
    IOCTL_STM_IDLE				= 0x2004,
    IOCTL_STM_WAKEUP			= 0x2005,
    IOCTL_STM_VIDIMMING			= 0x5001,
    IOCTL_STM_LEDFLASH			= 0x6001,
    IOCTL_STM_LEDMODE			= 0x6002,
    IOCTL_STM_READVER			= 0x7001,
    IOCTL_STM_READDDRREG		= 0x4001,
    IOCTL_STM_READDDRREG2		= 0x4002,
};


// =======================================================
// The /device/stm/immediate class
// -------------
class CWII_IPC_HLE_Device_stm_immediate : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_stm_immediate(u32 _DeviceID, const std::string& _rDeviceName) :
		IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
		{}

	virtual ~CWII_IPC_HLE_Device_stm_immediate()
	{}

	virtual bool Open(u32 _CommandAddress, u32 _Mode)
	{
		LOGV(WII_IPC_SD, 0, "STM: Open");
		Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
		return true;
	}

    virtual bool IOCtl(u32 _CommandAddress) 
    {
        u32 Parameter = Memory::Read_U32(_CommandAddress +0x0C);
		u32 BufferIn = Memory::Read_U32(_CommandAddress +0x10);
		u32 BufferInSize = Memory::Read_U32(_CommandAddress +0x14);
		u32 BufferOut = Memory::Read_U32(_CommandAddress +0x18);
		u32 BufferOutSize = Memory::Read_U32(_CommandAddress +0x1C);

		// Prepare the out buffer(s) with zeroes as a safety precaution
		// to avoid returning bad values
		Memory::Memset(BufferOut, 0, BufferOutSize);
		u32 ReturnValue = 0;

        switch(Parameter)
        {
        case IOCTL_STM_VIDIMMING: // (Input: 20 bytes, Output: 20 bytes)
            LOG(WII_IPC_SD, "%s - IOCtl: \n", GetDeviceName().c_str());
            LOG(WII_IPC_SD, "    IOCTL_STM_VIDIMMING");
			//DumpCommands(BufferIn, BufferInSize / 4, LogTypes::WII_IPC_SD);
			//Memory::Write_U32(1, BufferOut);
			//ReturnValue = 1;
            break;

        case IOCTL_STM_LEDMODE:  // (Input: 20 bytes, Output: 20 bytes)
            LOG(WII_IPC_SD, "%s - IOCtl: \n", GetDeviceName().c_str());
            LOG(WII_IPC_SD, "    IOCTL_STM_LEDMODE");
            break;

        default:
            {
                _dbg_assert_msg_(WII_IPC_SD, 0, "CWII_IPC_HLE_Device_stm_immediate: 0x%x", Parameter);

                LOG(WII_IPC_SD, "%s - IOCtl: \n", GetDeviceName().c_str());
                LOG(WII_IPC_SD, "    Parameter: 0x%x\n", Parameter);
                LOG(WII_IPC_SD, "    InBuffer: 0x%08x\n", BufferIn);
                LOG(WII_IPC_SD, "    InBufferSize: 0x%08x\n", BufferInSize);
                LOG(WII_IPC_SD, "    OutBuffer: 0x%08x\n", BufferOut);
                LOG(WII_IPC_SD, "    OutBufferSize: 0x%08x\n", BufferOutSize);
            }
            break;
        }

		// Write return value to the IPC call
        Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

		// Generate true or false reply for the main UpdateInterrupts() function
        return true;
    }
};


// =======================================================
// The /device/stm/eventhook class
// -------------
class CWII_IPC_HLE_Device_stm_eventhook : public IWII_IPC_HLE_Device
{
public:

    CWII_IPC_HLE_Device_stm_eventhook(u32 _DeviceID, const std::string& _rDeviceName) 
		: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
		, m_EventHookAddress(0)
    {}

    virtual ~CWII_IPC_HLE_Device_stm_eventhook()
    {}

    virtual bool Open(u32 _CommandAddress, u32 _Mode)
    {
        Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
        return true;
    }

    virtual bool IOCtl(u32 _CommandAddress) 
    {
        u32 Parameter = Memory::Read_U32(_CommandAddress +0x0C);
        u32 BufferIn = Memory::Read_U32(_CommandAddress +0x10);
        u32 BufferInSize = Memory::Read_U32(_CommandAddress +0x14);
        u32 BufferOut = Memory::Read_U32(_CommandAddress +0x18);
        u32 BufferOutSize = Memory::Read_U32(_CommandAddress +0x1C);

		// Prepare the out buffer(s) with zeroes as a safety precaution
		// to avoid returning bad values
		Memory::Memset(BufferOut, 0, BufferOutSize);
		u32 ReturnValue = 0;

        // write return value
        switch (Parameter)
        {
        case IOCTL_STM_EVENTHOOK:
			{
				m_EventHookAddress = _CommandAddress;

                LOG(WII_IPC_SD, "%s registers event hook:\n", GetDeviceName().c_str());
                LOG(WII_IPC_SD, "    0x1000 - IOCTL_STM_EVENTHOOK\n", Parameter);
                LOG(WII_IPC_SD, "    BufferIn: 0x%08x\n", BufferIn);
                LOG(WII_IPC_SD, "    BufferInSize: 0x%08x\n", BufferInSize);
                LOG(WII_IPC_SD, "    BufferOut: 0x%08x\n", BufferOut);
                LOG(WII_IPC_SD, "    BufferOutSize: 0x%08x\n", BufferOutSize);

				DumpCommands(BufferIn, BufferInSize/4, LogTypes::WII_IPC_SD);

                return false;
            }
            break;

        default:
            {
                _dbg_assert_msg_(WII_IPC_SD, 0, "CWII_IPC_HLE_Device_stm_eventhook: 0x%x", Parameter);
                LOG(WII_IPC_SD, "%s registers event hook:\n", GetDeviceName().c_str());
                LOG(WII_IPC_SD, "    Parameter: 0x%x\n", Parameter);
                LOG(WII_IPC_SD, "    BufferIn: 0x%08x\n", BufferIn);
                LOG(WII_IPC_SD, "    BufferInSize: 0x%08x\n", BufferInSize);
                LOG(WII_IPC_SD, "    BufferOut: 0x%08x\n", BufferOut);
                LOG(WII_IPC_SD, "    BufferOutSize: 0x%08x\n", BufferOutSize);
            }
            break;
        }

		// Write return value to the IPC call, 0 means success
        Memory::Write_U32(0, _CommandAddress + 0x4);   

		// Generate true or false reply for the main UpdateInterrupts() function
        return false;
	}

	// STATE_TO_SAVE
	u32 m_EventHookAddress;
};


#endif

