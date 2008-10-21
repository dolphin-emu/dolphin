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

#include <map>
#include <string>

#include "Common.h"
#include "WII_IPC_HLE.h"
#include "WII_IPC_HLE_Device.h"
#include "WII_IPC_HLE_Device_Error.h"
#include "WII_IPC_HLE_Device_DI.h"
#include "WII_IPC_HLE_Device_FileIO.h"
#include "WII_IPC_HLE_Device_stm.h"
#include "WII_IPC_HLE_Device_fs.h"
#include "WII_IPC_HLE_Device_net.h"
#include "WII_IPC_HLE_Device_es.h"
#include "WII_IPC_HLE_Device_usb.h"
#include "WII_IPC_HLE_Device_sdio_slot0.h"

#include "../HW/CPU.h"
#include "../HW/Memmap.h"
#include "../HW/WII_IPC.h"

#include "../Debugger/Debugger_SymbolMap.h"

namespace WII_IPC_HLE_Interface
{

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

typedef std::map<u32, IWII_IPC_HLE_Device*> TDeviceMap;
TDeviceMap g_DeviceMap;

// STATE_TO_SAVE
u32 g_LastDeviceID = 0x13370000;


void Init()
{
    _dbg_assert_msg_(WII_IPC, g_DeviceMap.empty(), "DeviceMap isnt empty on init");   
}

void Shutdown()
{
    TDeviceMap::const_iterator itr = g_DeviceMap.begin();
    while(itr != g_DeviceMap.end())
    {
        delete itr->second;
        ++itr;
    }
    g_DeviceMap.clear();
}

u32 GetDeviceIDByName(const std::string& _rDeviceName)
{
    TDeviceMap::const_iterator itr = g_DeviceMap.begin();
    while(itr != g_DeviceMap.end())
    {
        if (itr->second->GetDeviceName() == _rDeviceName)
            return itr->first;

        ++itr;
    }

    return 0;
}

IWII_IPC_HLE_Device* AccessDeviceByID(u32 _ID)
{
    if (g_DeviceMap.find(_ID) != g_DeviceMap.end())
        return g_DeviceMap[_ID];

    _dbg_assert_msg_(WII_IPC, 0, "IOP tries to access an unknown device 0x%x", _ID);

	return NULL;
}

void DeleteDeviceByID(u32 ID)
{
	IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(ID);
	if (pDevice != NULL)
	{
		delete pDevice;
		g_DeviceMap.erase(ID);
	}
}

IWII_IPC_HLE_Device* CreateDevice(u32 _DeviceID, const std::string& _rDeviceName)
{
	// scan device name and create the right one ^^
	IWII_IPC_HLE_Device* pDevice = NULL;
    if (_rDeviceName.find("/dev/") != std::string::npos)
    {
        if (_rDeviceName.c_str() == std::string("/dev/stm/immediate"))
        {
            pDevice = new CWII_IPC_HLE_Device_stm_immediate(_DeviceID, _rDeviceName);
        }
        else if (_rDeviceName.c_str() == std::string("/dev/stm/eventhook"))
        {
            pDevice = new CWII_IPC_HLE_Device_stm_eventhook(_DeviceID, _rDeviceName);			
        }
        else if (_rDeviceName.c_str() == std::string("/dev/di"))
        {
            pDevice = new CWII_IPC_HLE_Device_di(_DeviceID, _rDeviceName);			
        }
        else if (_rDeviceName.c_str() == std::string("/dev/fs"))
        {
            pDevice = new CWII_IPC_HLE_Device_fs(_DeviceID, _rDeviceName);			
        }
        else if (_rDeviceName.c_str() == std::string("/dev/net/kd/request"))
        {
            pDevice = new CWII_IPC_HLE_Device_net_kd_request(_DeviceID, _rDeviceName);			
        }
        else if (_rDeviceName.c_str() == std::string("/dev/net/kd/time"))
        {
            pDevice = new CWII_IPC_HLE_Device_net_kd_time(_DeviceID, _rDeviceName);			
        }
        else if (_rDeviceName.c_str() == std::string("/dev/es"))
        {
            pDevice = new CWII_IPC_HLE_Device_es(_DeviceID, _rDeviceName);			
        }
        else if (_rDeviceName.find("/dev/usb/oh1/57e/305") != std::string::npos)
        {
            pDevice = new CWII_IPC_HLE_Device_usb_oh1_57e_305(_DeviceID, _rDeviceName);
        }
		else if (_rDeviceName.find("/dev/sdio/slot0") != std::string::npos)
		{
			pDevice = new CWII_IPC_HLE_Device_sdio_slot0(_DeviceID, _rDeviceName);
		}
        else
        {
            PanicAlert("Unknown device: %s", _rDeviceName.c_str());
            pDevice = new CWII_IPC_HLE_Device_Error(u32(-1), _rDeviceName);
        }
    }
    else
    {
        pDevice = new CWII_IPC_HLE_Device_FileIO(_DeviceID, _rDeviceName);
    }

	return pDevice;
}

std::queue<u32> m_Ack;
std::queue<std::pair<u32,std::string> > m_ReplyQueue;

void ExecuteCommand(u32 _Address);
bool AckCommand(u32 _Address)
{   
/*    static u32 Count = 0;
    LOG(WII_IPC_HLE, "AckAdd: %i", Count);
    if (Count == 63)
        CCPU::Break();
    Count++; */

//	Debugger::PrintCallstack(LogTypes::WII_IPC_HLE);
//	LOG(WII_IPC_HLE, "AckCommand: 0%08x", _Address);

    m_Ack.push(_Address);

    return true;
}


void ExecuteCommand(u32 _Address)
{
/*    // small dump
    switch (Memory::Read_U32(_Address))
    {
    case COMMAND_OPEN_DEVICE:   LOG(WII_IPC_HLE, "COMMAND_OPEN_DEVICE"); break;
    case COMMAND_CLOSE_DEVICE:  LOG(WII_IPC_HLE, "COMMAND_CLOSE_DEVICE"); break;
    case COMMAND_READ:          LOG(WII_IPC_HLE, "COMMAND_READ"); break;
    case COMMAND_WRITE:         LOG(WII_IPC_HLE, "COMMAND_WRITE"); break;
    case COMMAND_SEEK:          LOG(WII_IPC_HLE, "COMMAND_SEEK"); break;
    case COMMAND_IOCTL:         LOG(WII_IPC_HLE, "COMMAND_IOCTL"); break;
    case COMMAND_IOCTLV:        LOG(WII_IPC_HLE, "COMMAND_IOCTLV"); break;
    }

    for (size_t i=0; i<0x30/4; i++)
    {
        LOG(WII_IPC_HLE, "Command%02i: 0x%08x", i, Memory::Read_U32(_Address + i*4));	
    }*/

    bool GenerateReply = false;

    ECommandType Command = static_cast<ECommandType>(Memory::Read_U32(_Address));
    switch (Command)
    {
    case COMMAND_OPEN_DEVICE:
        {
            // HLE - Create a new HLE device
            std::string DeviceName;
            Memory::GetString(DeviceName, Memory::Read_U32(_Address + 0xC));
#ifdef LOGGING
            u32 Mode = Memory::Read_U32(_Address+0x10);
#endif
           
            u32 DeviceID = GetDeviceIDByName(DeviceName);
            if (DeviceID == 0)
            {
                // create the new device
				// alternatively we could pre create all devices and put them in a directory tree structure
				// then this would just return a pointer to the wanted device.
                u32 CurrentDeviceID = g_LastDeviceID;
                IWII_IPC_HLE_Device* pDevice = CreateDevice(CurrentDeviceID, DeviceName);			
			    g_DeviceMap[CurrentDeviceID] = pDevice;
			    g_LastDeviceID++;
                                
                GenerateReply = pDevice->Open(_Address);
                LOG(WII_IPC_HLE, "IOP: Open (Device=%s, Mode=%i)", pDevice->GetDeviceName().c_str(), Mode);
            }
            else
            {
#ifdef LOGGING
                IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(DeviceID);
#endif

                // we have already opened this device
                Memory::Write_U32(u32(-6), _Address + 4);
                GenerateReply = true;

                LOG(WII_IPC_HLE, "IOP: ReOpen (Device=%s, Mode=%i)", pDevice->GetDeviceName().c_str(), Mode);
            } 
        }
        break;

    case COMMAND_CLOSE_DEVICE:
        {
            u32 DeviceID = Memory::Read_U32(_Address+8);

            IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(DeviceID);
			if (pDevice != NULL)
			{
                // pDevice->Close(_Address);

				LOG(WII_IPC_HLE, "IOP: Close (Device=%s ID=0x%08x)", pDevice->GetDeviceName().c_str(), DeviceID);				

                DeleteDeviceByID(DeviceID);

                GenerateReply = true;
			}            
        }
        break;

    case COMMAND_READ:
        {
            u32 DeviceID = Memory::Read_U32(_Address+8);
            IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(DeviceID);
			if (pDevice != NULL)
				GenerateReply = pDevice->Read(_Address);
        }
        break;
    
    case COMMAND_WRITE:
        {
            u32 DeviceID = Memory::Read_U32(_Address+8);
            IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(DeviceID);
			if (pDevice != NULL)
				GenerateReply = pDevice->Write(_Address);
        }
        break;

    case COMMAND_SEEK:
        {
            u32 DeviceID = Memory::Read_U32(_Address+8);
            IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(DeviceID);
			if (pDevice != NULL)
				GenerateReply = pDevice->Seek(_Address);
        }
        break;

    case COMMAND_IOCTL:
        {
            u32 DeviceID = Memory::Read_U32(_Address+8);
            IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(DeviceID);
			if (pDevice != NULL)
				GenerateReply = pDevice->IOCtl(_Address);
        }
        break;

    case COMMAND_IOCTLV:
        {
            u32 DeviceID = Memory::Read_U32(_Address+8);
            IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(DeviceID);
			if (pDevice)
				GenerateReply = pDevice->IOCtlV(_Address);
        }
        break;

    default:
        _dbg_assert_msg_(WII_IPC_HLE, 0, "Unknown Command %i (0x%08x)", Command, _Address);
        CCPU::Break();
        break;
    }

    // seems that the org hw overwrites the command after it has been executed
    Memory::Write_U32(8, _Address);

    if (GenerateReply)
    {
        u32 DeviceID = Memory::Read_U32(_Address+8);

        IWII_IPC_HLE_Device* pDevice = NULL;
        if (g_DeviceMap.find(DeviceID) != g_DeviceMap.end())
            pDevice = g_DeviceMap[DeviceID];


        if (pDevice != NULL)
            m_ReplyQueue.push(std::pair<u32, std::string>(_Address, pDevice->GetDeviceName()));        
        else
            m_ReplyQueue.push(std::pair<u32, std::string>(_Address, "unknown"));        
    }
}

// Update
void Update()
{    
	if (WII_IPCInterface::IsReady())    
	{
        // check if an executed must be updated
	    TDeviceMap::const_iterator itr = g_DeviceMap.begin();
	    while(itr != g_DeviceMap.end())
	    {
		    u32 CommandAddr = itr->second->Update();
		    if (CommandAddr != 0)
		    {
                m_ReplyQueue.push(std::pair<u32, std::string>(CommandAddr, itr->second->GetDeviceName()));        
		    }
		    ++itr;
	    }

        // check if we have to execute an acknowledged command
        if (!m_ReplyQueue.empty())
        {
            LOG(WII_IPC_HLE + 100, "-- Generate Reply %s (0x%08x)", m_ReplyQueue.front().second.c_str(), m_ReplyQueue.front().first);
            WII_IPCInterface::GenerateReply(m_ReplyQueue.front().first);
            m_ReplyQueue.pop();
            return;
        }



        if (m_ReplyQueue.empty() && !m_Ack.empty())
        {
            u32 _Address = m_Ack.front();
            m_Ack.pop();
            ExecuteCommand(_Address);
            LOG(WII_IPC_HLE + 100, "-- Generate Ack (0x%08x)", _Address);
            WII_IPCInterface::GenerateAck(_Address);
        }
	}
}

} // end of namespace IPC
