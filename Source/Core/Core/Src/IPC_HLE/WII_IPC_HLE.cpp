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



// =======================================================
// File description
// -------------
/* This is the main Wii IPC file that handles all incoming IPC calls and directs them
   to the right function.
   
   IPC basics:
   
   Return values for file handles: All IPC calls will generate a return value to 0x04,
   in case of success they are
		Open: DeviceID
		Close: 0
		Read: Bytes read
		Write: Bytes written
		Seek: Seek position
		Ioctl: 0 (in addition to that there may be messages to the out buffers)
		Ioctlv: 0 (in addition to that there may be messages to the out buffers)
   They will also generate a true or false return for UpdateInterrupts() in WII_IPC.cpp. */
// =============




#include <map>
#include <string>
#include <list>

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

#include "FileUtil.h" // For Copy
#include "../Core.h"
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
std::list<u32> g_Ack;
std::queue<std::pair<u32,std::string> > g_ReplyQueue;
void ExecuteCommand(u32 _Address);

// General IPC functions 
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

// This is called from COMMAND_OPEN_DEVICE. Here we either create a new device
//   or open a new file handle.
IWII_IPC_HLE_Device* CreateDevice(u32 _DeviceID, const std::string& _rDeviceName)
{
	// scan device name and create the right one
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
		else if (_rDeviceName.c_str() == std::string("/dev/net/ncd/manage"))
		{
			pDevice = new CWII_IPC_HLE_Device_net_ncd_manage(_DeviceID, _rDeviceName);			
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
		LOGV(WII_IPC_FILEIO, 0, "IOP: Create Device %s", _rDeviceName.c_str());
        pDevice = new CWII_IPC_HLE_Device_FileIO(_DeviceID, _rDeviceName);
    }

	return pDevice;
}

// ===================================================
/* This generates an acknowledgment to IPC calls. This function is called from
   IPC_CONTROL_REGISTER requests in WII_IPC.cpp. The acknowledgment _Address will
   start with 0x033e...., it will be for the _CommandAddress 0x133e...., from
   debugging I also noticed that the Ioctl arguments are stored temporarily in
   0x933e.... with the same .... as in the _CommandAddress. */
// ----------------
bool AckCommand(u32 _Address)
{   
	// Debugger::PrintCallstack(LogTypes::WII_IPC_HLE);
	LOG(WII_IPC_HLE, "AckCommand: 0%08x", _Address);

	std::list<u32>::iterator itr = g_Ack.begin();
	while (itr != g_Ack.end())
	{
		if (*itr == _Address)
		{
			PanicAlert("execute a command two times");
			return false;
		}

		itr++;
	}

    g_Ack.push_back(_Address);

    return true;
}

// Let the game read the setting.txt file
void CopySettingsFile(std::string DeviceName)
{
	std::string Source;
	if(Core::GetStartupParameter().bNTSC)
		Source = "Sys/Wii/setting-usa.txt";
	else
		Source = "Sys/Wii/setting-eur.txt";

	std::string Target = "User/Wii" + DeviceName;

	// Check if the target dir exists, otherwise create it
	std::string TargetDir = Target.substr(0, Target.find_last_of("/"));
	if(!File::IsDirectory(TargetDir.c_str())) File::CreateDirectoryStructure(Target.c_str());

	if (File::Copy(Source.c_str(), Target.c_str()))
	{
		LOG(WII_IPC_FILEIO, "FS: Copied %s to %s", Source.c_str(), Target.c_str());
	}
	else
	{
		LOG(WII_IPC_FILEIO, "Could not copy %s to %s", Source.c_str(), Target.c_str());
		PanicAlert("Could not copy %s to %s", Source.c_str(), Target.c_str());
	}
}

void ExecuteCommand(u32 _Address)
{
    bool GenerateReply = false;

    ECommandType Command = static_cast<ECommandType>(Memory::Read_U32(_Address));
    switch (Command)
    {
    case COMMAND_OPEN_DEVICE:
        {
            // Create a new HLE device. The Mode and DeviceName is given to us but we
			//   generate a DeviceID to be used for access to this device until it is Closed.
            std::string DeviceName;
            Memory::GetString(DeviceName, Memory::Read_U32(_Address + 0xC));

			// The game may try to read setting.txt here, in that case copy it so it can read it
			if(DeviceName.find("setting.txt") != std::string::npos) CopySettingsFile(DeviceName);

            u32 Mode = Memory::Read_U32(_Address + 0x10);
            u32 DeviceID = GetDeviceIDByName(DeviceName);
			
			// check if a device with this name has been created already
            if (DeviceID == 0)				
            {
                // create the new device
				// alternatively we could pre create all devices and put them in a directory tree structure
				// then this would just return a pointer to the wanted device.
                u32 CurrentDeviceID = g_LastDeviceID;
                IWII_IPC_HLE_Device* pDevice = CreateDevice(CurrentDeviceID, DeviceName);			
			    g_DeviceMap[CurrentDeviceID] = pDevice;
			    g_LastDeviceID++;
                                
                GenerateReply = pDevice->Open(_Address, Mode);
				if(pDevice->GetDeviceName().find("/dev/") == std::string::npos
					|| pDevice->GetDeviceName().c_str() == std::string("/dev/fs"))
				{					
					LOG(WII_IPC_FILEIO, "IOP: Open (Device=%s, DeviceID=%08x, Mode=%i, GenerateReply=%i)",
						pDevice->GetDeviceName().c_str(), CurrentDeviceID, Mode, (int)GenerateReply);
				}
				else
				{
					LOG(WII_IPC_HLE, "IOP: Open (Device=%s, Mode=%i)",
						pDevice->GetDeviceName().c_str(), Mode);
				}
            }
            else
            {
				// The device has already been opened and was not closed, reuse the same DeviceID. 

                IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(DeviceID);
                // If we return -6 here after a Open > Failed > CREATE_FILE > ReOpen call
				//   sequence Mario Galaxy and Mario Kart Wii will not start writing to the file,
				//   it will just (seemingly) wait for one or two seconds and then give an error
				//   message. So I'm trying to return the DeviceID instead to make it write to the file.
				//   (Which was most likely the reason it created the file in the first place.) */

				// F|RES: prolly the re-open is just a mode change

				if(DeviceName.find("/dev/") == std::string::npos)
				{
					LOG(WII_IPC_FILEIO, "IOP: ReOpen (Device=%s, DeviceID=%08x, Mode=%i)",
						pDevice->GetDeviceName().c_str(), DeviceID, Mode);

					u32 Mode = Memory::Read_U32(_Address + 0x10);

					// We may not have a file handle at this point, in Mario Kart I got a
					//  Open > Failed > ... other stuff > ReOpen call sequence, in that case
					//  we have no file and no file handle, so we call Open again to basically
					//  get a -106 error so that the game call CreateFile and then ReOpen again. 
					if(pDevice->ReturnFileHandle())
						Memory::Write_U32(DeviceID, _Address + 4);
					else
						GenerateReply = pDevice->Open(_Address, Mode);						
				}
				else
				{
					LOG(WII_IPC_HLE, "IOP: ReOpen (Device=%s, DeviceID=%08x, Mode=%i)",
						pDevice->GetDeviceName().c_str(), pDevice->GetDeviceID(), Mode);

					// We have already opened this device, return -6
					Memory::Write_U32(u32(-6), _Address + 4);
				}
                GenerateReply = true;                
            } 
        }
        break;

    case COMMAND_CLOSE_DEVICE:
        {
            u32 DeviceID = Memory::Read_U32(_Address + 8);

            IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(DeviceID);
			if (pDevice != NULL)
			{
                pDevice->Close(_Address);

				// Delete the device when CLOSE is called, this does not effect
				// GenerateReply() for any other purpose than the logging because
				// it's a true / false only function //
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

    // It seems that the original hardware overwrites the command after it has been
	//	   executed. We write 8 which is not any valid command. 
	Memory::Write_U32(8, _Address);

	// Generate a reply to the IPC command
    if (GenerateReply)
    {
		// Get device id
		u32 DeviceID = Memory::Read_U32(_Address + 8);
        IWII_IPC_HLE_Device* pDevice = NULL;

		// Get the device from the device map
        if (g_DeviceMap.find(DeviceID) != g_DeviceMap.end())
            pDevice = g_DeviceMap[DeviceID];

        if (pDevice != NULL)
		{
			// Write reply, this will later be executed in Update()
			g_ReplyQueue.push(std::pair<u32, std::string>(_Address, pDevice->GetDeviceName()));
		}
        else
		{
            g_ReplyQueue.push(std::pair<u32, std::string>(_Address, "unknown")); 
		}
    }
}

// This is called continuously and WII_IPCInterface::IsReady() is controlled from WII_IPC.cpp. 
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
                g_ReplyQueue.push(std::pair<u32, std::string>(CommandAddr, itr->second->GetDeviceName()));        
		    }
		    ++itr;
	    }

        // Check if we have to execute an acknowledge command...
        if (!g_ReplyQueue.empty())
        {
			WII_IPCInterface::GenerateReply(g_ReplyQueue.front().first);
            g_ReplyQueue.pop();
            return;
        }

		// ...no we don't, we can now execute the IPC command
        if (g_ReplyQueue.empty() && !g_Ack.empty())
        {
            u32 _Address = g_Ack.front();
            g_Ack.pop_front();
            ExecuteCommand(_Address);
            LOGV(WII_IPC_HLE, 1, "-- Generate Ack (0x%08x)", _Address);

			// Go back to WII_IPC.cpp and generate an acknowledgement
            WII_IPCInterface::GenerateAck(_Address);
        }
	}
}

} // end of namespace IPC
