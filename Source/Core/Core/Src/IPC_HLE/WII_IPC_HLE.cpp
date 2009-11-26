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
#include "WII_IPC_HLE_Device_usb_kbd.h"
#include "WII_IPC_HLE_Device_sdio_slot0.h"

#include "FileUtil.h" // For Copy
#include "../Core.h"
#include "../HW/CPU.h"
#include "../HW/Memmap.h"
#include "../HW/WII_IPC.h"
#include "../Debugger/Debugger_SymbolMap.h"
#include "../PowerPC/PowerPC.h"


namespace WII_IPC_HLE_Interface
{

typedef std::map<u32, IWII_IPC_HLE_Device*> TDeviceMap;
TDeviceMap g_DeviceMap;

// STATE_TO_SAVE
typedef std::map<u32, std::string> TFileNameMap;
TFileNameMap g_FileNameMap;
u32 g_LastDeviceID;

// General IPC functions 
void Init()
{
    _dbg_assert_msg_(WII_IPC_HLE, g_DeviceMap.empty(), "DeviceMap isnt empty on init");

	u32 i = IPC_FIRST_HARDWARE_ID;
	// Build hardware devices
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_oh1_57e_305(i, std::string("/dev/usb/oh1/57e/305")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stm_immediate(i, std::string("/dev/stm/immediate")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stm_eventhook(i, std::string("/dev/stm/eventhook")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_fs(i, std::string("/dev/fs")); i++;
	// Warning: "/dev/es" must be created after "/dev/fs", not before
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_es(i, std::string("/dev/es")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_di(i, std::string("/dev/di")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_kd_request(i, std::string("/dev/net/kd/request")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_kd_time(i, std::string("/dev/net/kd/time")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_ncd_manage(i, std::string("/dev/net/ncd/manage")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_ip_top(i, std::string("/dev/net/ip/top")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_oh0(i, std::string("/dev/usb/oh0")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_kbd(i, std::string("/dev/usb/kbd")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_sdio_slot0(i, std::string("/dev/sdio/slot0")); i++;
	//g_DeviceMap[i] = new CWII_IPC_HLE_Device_Error(i, std::string("_Unknown_Device_")); i++;

	g_LastDeviceID = IPC_FIRST_FILEIO_ID;
}

void Reset(bool _hard)
{
    TDeviceMap::const_iterator itr = g_DeviceMap.begin();
    while (itr != g_DeviceMap.end())
    {
		if (itr->second)
		{
			if (_hard||(!itr->second->IsHardware()))
			{
				// Hardware should not be deleted unless it is a hard reset
				delete itr->second;
				g_DeviceMap.erase(itr->first);
			}
		}
		else
		{
			// Erase invalid device
			g_DeviceMap.erase(itr->first);
		}
		++itr;
    }
	g_FileNameMap.clear();

    g_LastDeviceID = IPC_FIRST_FILEIO_ID;
}

void Shutdown()
{
    Reset(true);
}

// Set default content file
void SetDefaultContentFile(const std::string& _rFilename)
{
	CWII_IPC_HLE_Device_es* pDevice = (CWII_IPC_HLE_Device_es*)AccessDeviceByID(GetDeviceIDByName(std::string("/dev/es")));
	if (pDevice)
		pDevice->Load(_rFilename);
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

	return NULL;
}

void DeleteDeviceByID(u32 ID)
{
	IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(ID);
	if (pDevice)
		delete pDevice;

	g_DeviceMap.erase(ID);
	g_FileNameMap.erase(ID);
}

// This is called from COMMAND_OPEN_DEVICE. Here we either create a new file handle
IWII_IPC_HLE_Device* CreateFileIO(u32 _DeviceID, const std::string& _rDeviceName)
{
	// scan device name and create the right one
	IWII_IPC_HLE_Device* pDevice = NULL;

	INFO_LOG(WII_IPC_FILEIO, "IOP: Create FileIO %s", _rDeviceName.c_str());
	pDevice = new CWII_IPC_HLE_Device_FileIO(_DeviceID, _rDeviceName);

	return pDevice;
}

// Let the game read the setting.txt file
void CopySettingsFile(std::string& DeviceName)
{
	std::string Source = File::GetSysDirectory() + WII_SYS_DIR + DIR_SEP;
	if(Core::GetStartupParameter().bNTSC)
		Source += "setting-usa.txt";
	else
		Source += "setting-eur.txt";

	std::string Target = FULL_WII_ROOT_DIR + DeviceName;

	// Check if the target dir exists, otherwise create it
	std::string TargetDir = Target.substr(0, Target.find_last_of(DIR_SEP));
	if(!File::IsDirectory(TargetDir.c_str())) File::CreateFullPath(Target.c_str());

	if (File::Copy(Source.c_str(), Target.c_str()))
	{
		INFO_LOG(WII_IPC_FILEIO, "FS: Copied %s to %s", Source.c_str(), Target.c_str());
	}
	else
	{
		ERROR_LOG(WII_IPC_FILEIO, "Could not copy %s to %s", Source.c_str(), Target.c_str());
		PanicAlert("Could not copy %s to %s", Source.c_str(), Target.c_str());
	}
}

void DoState(PointerWrap &p)
{
	p.Do(g_LastDeviceID);

	// Currently only USB device needs to be saved
	IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(GetDeviceIDByName(std::string("/dev/usb/oh1/57e/305")));
	if (pDevice)
		pDevice->DoState(p);
	else
		PanicAlert("WII_IPC_HLE: Save/Load State failed, Device /dev/usb/oh1/57e/305 doesn't exist!");

	if (p.GetMode() == PointerWrap::MODE_READ)
	{	
		// Delete file Handles
		TFileNameMap::const_iterator itr = g_FileNameMap.begin();
		while (itr != g_FileNameMap.end())
		{
			delete g_DeviceMap[itr->first];
			g_DeviceMap.erase(itr->first);
			++itr;
		}
		// Load file names
		p.Do(g_FileNameMap);
		// Rebuild file handles
		itr = g_FileNameMap.begin();
		while (itr != g_FileNameMap.end())
		{
			g_DeviceMap[itr->first] = new CWII_IPC_HLE_Device_FileIO(itr->first, itr->second);
			++itr;
		}
	}
	else
	{
		p.Do(g_FileNameMap);
	}
}

// ===================================================
/* This generates an acknowledgment to IPC calls. This function is called from
   IPC_CONTROL_REGISTER requests in WII_IPC.cpp. The acknowledgment _Address will
   start with 0x033e...., it will be for the _CommandAddress 0x133e...., from
   debugging I also noticed that the Ioctl arguments are stored temporarily in
   0x933e.... with the same .... as in the _CommandAddress. */
// ----------------

void ExecuteCommand(u32 _Address)
{
    bool CmdSuccess = false;

    ECommandType Command = static_cast<ECommandType>(Memory::Read_U32(_Address));
	u32 DeviceID = Memory::Read_U32(_Address + 8);
	IWII_IPC_HLE_Device* pDevice = AccessDeviceByID(DeviceID);

	INFO_LOG(WII_IPC_HLE, "-->> Execute Command Address: 0x%08x (code: %x, device: %x) ", _Address, Command, DeviceID);

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
            DeviceID = GetDeviceIDByName(DeviceName);

			// check if a device with this name has been created already
            if (DeviceID == 0)				
            {
				if (DeviceName.find("/dev/") != std::string::npos)
				{
					ERROR_LOG(WII_IPC_FILEIO, "Unknown device: %s", DeviceName.c_str());
				 	PanicAlert("Unknown device: %s", DeviceName.c_str());
				}
				else
				{
					// create new file handle
		            u32 CurrentDeviceID = g_LastDeviceID;
			        pDevice = CreateFileIO(CurrentDeviceID, DeviceName);			
				    g_DeviceMap[CurrentDeviceID] = pDevice;
					g_FileNameMap[CurrentDeviceID] = DeviceName;
					g_LastDeviceID++;
                                
					CmdSuccess = pDevice->Open(_Address, Mode);

					INFO_LOG(WII_IPC_FILEIO, "IOP: Open File (Device=%s, ID=%08x, Mode=%i)",
							pDevice->GetDeviceName().c_str(), CurrentDeviceID, Mode);
				}
            }
            else
            {
				CmdSuccess = true;
				// The device is already created 
                pDevice = AccessDeviceByID(DeviceID);

				// If we return -6 here after a Open > Failed > CREATE_FILE > ReOpen call
				//   sequence Mario Galaxy and Mario Kart Wii will not start writing to the file,
				//   it will just (seemingly) wait for one or two seconds and then give an error
				//   message. So I'm trying to return the DeviceID instead to make it write to the file.
				//   (Which was most likely the reason it created the file in the first place.)

				// F|RES: prolly the re-open is just a mode change

                INFO_LOG(WII_IPC_FILEIO, "IOP: ReOpen (Device=%s, DeviceID=%08x, Mode=%i)",
                    pDevice->GetDeviceName().c_str(), DeviceID, Mode);

				if(pDevice->IsHardware())
				{
					// We have already opened this device, return -6
					if(pDevice->IsOpened())
						Memory::Write_U32(u32(-6), _Address + 4);
					else
						pDevice->Open(_Address, Mode);
				}
				else
				{
					// We may not have a file handle at this point, in Mario Kart I got a
					//  Open > Failed > ... other stuff > ReOpen call sequence, in that case
					//  we have no file and no file handle, so we call Open again to basically
					//  get a -106 error so that the game call CreateFile and then ReOpen again. 
					if(pDevice->ReturnFileHandle())
						Memory::Write_U32(DeviceID, _Address + 4);
					else
						pDevice->Open(_Address, Mode);						
				}                
            }
        }
        break;

    case COMMAND_CLOSE_DEVICE:
        {
            if (pDevice)
			{
                pDevice->Close(_Address);
				// Don't delete hardware
				if (!pDevice->IsHardware())
					DeleteDeviceByID(DeviceID);
                CmdSuccess = true;
			}
        }
        break;

    case COMMAND_READ:
        {
			if (pDevice != NULL)
				CmdSuccess = pDevice->Read(_Address);
        }
        break;
    
    case COMMAND_WRITE:
        {
			if (pDevice != NULL)
				CmdSuccess = pDevice->Write(_Address);
        }
        break;

    case COMMAND_SEEK:
        {
			if (pDevice != NULL)
				CmdSuccess = pDevice->Seek(_Address);
        }
        break;

    case COMMAND_IOCTL:
        {
			if (pDevice != NULL)
				CmdSuccess = pDevice->IOCtl(_Address);
        }
        break;

    case COMMAND_IOCTLV:
        {
			if (pDevice)
				CmdSuccess = pDevice->IOCtlV(_Address);
        }
        break;

    default:
        _dbg_assert_msg_(WII_IPC_HLE, 0, "Unknown IPC Command %i (0x%08x)", Command, _Address);
		// Break on the same terms as the _dbg_assert_msg_, if LOGGING was defined
        break;
    }


    // It seems that the original hardware overwrites the command after it has been
	//	   executed. We write 8 which is not any valid command. 
	//
	// AyuanX: Is this really necessary?
	// My experiment says no, so I'm just commenting this out
	//
	//Memory::Write_U32(8, _Address);

    if (CmdSuccess)
    {
		// Generate a reply to the IPC command
		WII_IPCInterface::EnqReply(_Address);
    }
	else
	{
		//INFO_LOG(WII_IPC_HLE, "<<-- Failed or Not Ready to Reply to Command Address: 0x%08x ", _Address);
	}
}


// ===================================================
// This is called continuously from SystemTimers.cpp
// ---------------------------------------------------
void Update()
{
	if (WII_IPCInterface::IsReady() == false)
		return;

	UpdateDevices();

	// if we have a reply to send
	u32 _Reply = WII_IPCInterface::DeqReply();
	if (_Reply != NULL)
	{
		WII_IPCInterface::GenerateReply(_Reply);
		INFO_LOG(WII_IPC_HLE, "<<-- Reply to Command Address: 0x%08x", _Reply);
		return;
	}

	// If there is a a new command
	u32 _Address = WII_IPCInterface::GetAddress();
	if (_Address != NULL)
	{
		WII_IPCInterface::GenerateAck();
		INFO_LOG(WII_IPC_HLE, "||-- Acknowledge Command Address: 0x%08x", _Address);

		ExecuteCommand(_Address);

		// AyuanX: Since current HLE time slot is empty, we can piggyback a reply
		// Besides, this trick makes a Ping-Pong Reply FIFO never get full
		// I don't know whether original hardware supports this feature or not
		// but it works here and we gain 1/3 extra bandwidth
		//
		u32 _Reply = WII_IPCInterface::DeqReply();
		if (_Reply != NULL)
		{
			WII_IPCInterface::GenerateReply(_Reply);
			INFO_LOG(WII_IPC_HLE, "<<-- Reply to Command Address: 0x%08x", _Reply);
		}

		#if MAX_LOG_LEVEL >= DEBUG_LEVEL
			Debugger::PrintCallstack(LogTypes::WII_IPC_HLE, LogTypes::LDEBUG);
		#endif

		return;
	}

}

void UpdateDevices()
{
	// This is called frequently so better make this route simple
	// currently we only have one device: USB that needs update and its ID is fixed to IPC_FIRST_HARDWARE_ID
	// So let's skip the query and call it directly, which will speed up a lot
	//
	IWII_IPC_HLE_Device* pDevice = g_DeviceMap[IPC_FIRST_HARDWARE_ID];
	if (pDevice->IsOpened())
		pDevice->Update();
	/*
	// check if a device must be updated
	TDeviceMap::const_iterator itr = g_DeviceMap.begin();

	while(itr != g_DeviceMap.end())
	{
		if (itr->second->Update())
		{
			break;
		}
		++itr;
	}
	*/
}


} // end of namespace IPC
