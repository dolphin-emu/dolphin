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

/*
This is the main Wii IPC file that handles all incoming IPC calls and directs them
to the right function.

IPC basics (IOS' usage):

Return values for file handles: All IPC calls will generate a return value to 0x04,
in case of success they are
	Open: DeviceID
	Close: 0
	Read: Bytes read
	Write: Bytes written
	Seek: Seek position
	Ioctl: 0 (in addition to that there may be messages to the out buffers)
	Ioctlv: 0 (in addition to that there may be messages to the out buffers)
They will also generate a true or false return for UpdateInterrupts() in WII_IPC.cpp.
*/

#include <map>
#include <string>
#include <list>

#include "Common.h"
#include "CommonPaths.h"
#include "WII_IPC_HLE.h"
#include "WII_IPC_HLE_Device.h"
#include "WII_IPC_HLE_Device_DI.h"
#include "WII_IPC_HLE_Device_FileIO.h"
#include "WII_IPC_HLE_Device_stm.h"
#include "WII_IPC_HLE_Device_fs.h"
#include "WII_IPC_HLE_Device_net.h"
#include "WII_IPC_HLE_Device_es.h"
#include "WII_IPC_HLE_Device_usb.h"
#include "WII_IPC_HLE_Device_usb_kbd.h"
#include "WII_IPC_HLE_WiiSpeak.h"
#include "WII_IPC_HLE_Device_sdio_slot0.h"

#include "FileUtil.h" // For Copy
#include "../ConfigManager.h"
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

typedef std::deque<u32> ipc_msg_queue;
static ipc_msg_queue request_queue;	// ppc -> arm
static ipc_msg_queue reply_queue;	// arm -> ppc

void Init()
{
    _dbg_assert_msg_(WII_IPC_HLE, g_DeviceMap.empty(), "DeviceMap isnt empty on init");

	u32 i = IPC_FIRST_HARDWARE_ID;
	// Build hardware devices
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_oh0(i, std::string("/dev/usb/oh0")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stub(i, std::string("/dev/usb/hid")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_ven(i, std::string("/dev/usb/ven")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_kbd(i, std::string("/dev/usb/kbd")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_oh1_57e_305(i, std::string("/dev/usb/oh1/57e/305")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_oh0_57e_308(i, std::string("/dev/usb/oh0/57e/308")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_oh0_46d_a03(i, std::string("/dev/usb/oh0/46d/a03")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stm_immediate(i, std::string("/dev/stm/immediate")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stm_eventhook(i, std::string("/dev/stm/eventhook")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_fs(i, std::string("/dev/fs")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_es(i, std::string("/dev/es")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_di(i, std::string("/dev/di")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_kd_request(i, std::string("/dev/net/kd/request")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_kd_time(i, std::string("/dev/net/kd/time")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_ncd_manage(i, std::string("/dev/net/ncd/manage")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_ip_top(i, std::string("/dev/net/ip/top")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_sdio_slot0(i, std::string("/dev/sdio/slot0")); i++;
    g_DeviceMap[i] = new CWII_IPC_HLE_Device_sdio_slot0(i, std::string("/dev/sdio/slot1")); i++;
    g_DeviceMap[i] = new CWII_IPC_HLE_Device_sdio_slot0(i, std::string("/dev/sdio/slot2")); i++;
	g_DeviceMap[i] = new IWII_IPC_HLE_Device(i, std::string("_Unimplemented_Device_"));

	g_LastDeviceID = IPC_FIRST_FILEIO_ID;
}

void Reset(bool _bHard)
{
    TDeviceMap::iterator itr = g_DeviceMap.begin();
    while (itr != g_DeviceMap.end())
    {
		if (itr->second)
		{
			// Force close
			itr->second->Close(0, true);
			// Hardware should not be deleted unless it is a hard reset
			if (_bHard || !itr->second->IsHardware())
				delete itr->second;
		}
		++itr;
    }
	// Skip hardware devices if not a hard reset
	itr = (_bHard) ? g_DeviceMap.begin() : g_DeviceMap.lower_bound(IPC_FIRST_FILEIO_ID);
	// Erase devices
	g_DeviceMap.erase(itr, g_DeviceMap.end());
	g_FileNameMap.clear();

	request_queue.clear();
	reply_queue.clear();

    g_LastDeviceID = IPC_FIRST_FILEIO_ID;
}

void Shutdown()
{
    Reset(true);
}

void SetDefaultContentFile(const std::string& _rFilename)
{
	CWII_IPC_HLE_Device_es* pDevice =
		(CWII_IPC_HLE_Device_es*)AccessDeviceByID(GetDeviceIDByName(std::string("/dev/es")));
	if (pDevice)
		pDevice->LoadWAD(_rFilename);
}

void ES_DIVerify(u8 *_pTMD, u32 _sz)
{
	CWII_IPC_HLE_Device_es* pDevice =
		(CWII_IPC_HLE_Device_es*)AccessDeviceByID(GetDeviceIDByName(std::string("/dev/es")));
	if (pDevice)
		pDevice->ES_DIVerify(_pTMD, _sz);
	else
		ERROR_LOG(WII_IPC_ES, "DIVerify called but /dev/es is not available");
}

void SDIO_EventNotify()
{
	CWII_IPC_HLE_Device_sdio_slot0 *pDevice =
		(CWII_IPC_HLE_Device_sdio_slot0*)AccessDeviceByID(GetDeviceIDByName(std::string("/dev/sdio/slot0")));
	if (pDevice)
		pDevice->EventNotify();
}

int GetDeviceIDByName(const std::string& _rDeviceName)
{
    TDeviceMap::const_iterator itr = g_DeviceMap.begin();
    while(itr != g_DeviceMap.end())
    {
        if (itr->second->GetDeviceName() == _rDeviceName)
            return itr->first;
        ++itr;
    }

    return -1;
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

// This is called from ExecuteCommand() COMMAND_OPEN_DEVICE
IWII_IPC_HLE_Device* CreateFileIO(u32 _DeviceID, const std::string& _rDeviceName)
{
	// scan device name and create the right one
	IWII_IPC_HLE_Device* pDevice = NULL;

	INFO_LOG(WII_IPC_FILEIO, "IOP: Create FileIO %s", _rDeviceName.c_str());
	pDevice = new CWII_IPC_HLE_Device_FileIO(_DeviceID, _rDeviceName);

	return pDevice;
}


void DoState(PointerWrap &p)
{
	p.Do(g_LastDeviceID);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{	
		TFileNameMap::const_iterator itr;
		// Delete file Handles
		itr = g_FileNameMap.begin();
		while (itr != g_FileNameMap.end())
		{
			TDeviceMap::const_iterator devitr = g_DeviceMap.find(itr->first);
			if (devitr != g_DeviceMap.end())
			{
				if (devitr->second)
					delete devitr->second;
				g_DeviceMap.erase(itr->first);
			}
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

	p.Do(request_queue);
	p.Do(reply_queue);


	TDeviceMap::const_iterator itr;
	//first, all the real devices
	//(because we need fs to be deserialized first)
	itr = g_DeviceMap.begin();
	while (itr != g_DeviceMap.end())
	{
		if (itr->second->IsHardware())
			itr->second->DoState(p);
		++itr;
	}
	//then all the files
	itr = g_DeviceMap.begin();
	while (itr != g_DeviceMap.end())
	{
		if (!itr->second->IsHardware())
			itr->second->DoState(p);
		++itr;
	}
}

void ExecuteCommand(u32 _Address)
{
    bool CmdSuccess = false;

    ECommandType Command = static_cast<ECommandType>(Memory::Read_U32(_Address));
	int DeviceID = Memory::Read_U32(_Address + 8);
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

            u32 Mode = Memory::Read_U32(_Address + 0x10);
            DeviceID = GetDeviceIDByName(DeviceName);

            // check if a device with this name has been created already
            if (DeviceID == -1)				
            {
				if (DeviceName.find("/dev/") != std::string::npos)
				{
					WARN_LOG(WII_IPC_HLE, "Unimplemented device: %s", DeviceName.c_str());

					pDevice = AccessDeviceByID(GetDeviceIDByName(std::string("_Unimplemented_Device_")));
					CmdSuccess = pDevice->Open(_Address, Mode);
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
				// F|RES: prolly the re-open is just a mode change
                pDevice = AccessDeviceByID(DeviceID);
				CmdSuccess = pDevice->Open(_Address, Mode);

                INFO_LOG(WII_IPC_FILEIO, "IOP: ReOpen (Device=%s, DeviceID=%08x, Mode=%i)",
                    pDevice->GetDeviceName().c_str(), DeviceID, Mode);
            }
        }
        break;

    case COMMAND_CLOSE_DEVICE:
        if (pDevice)
		{
            CmdSuccess = pDevice->Close(_Address);
			// Don't delete hardware
			if (!pDevice->IsHardware())
				DeleteDeviceByID(DeviceID);
		}
		else
		{
			Memory::Write_U32(FS_EINVAL, _Address + 4);
			CmdSuccess = true;
		}
        break;

    case COMMAND_READ:
		if (pDevice)
			CmdSuccess = pDevice->Read(_Address);
		else
		{
			Memory::Write_U32(FS_EINVAL, _Address + 4);
			CmdSuccess = true;
		}
        break;
    
    case COMMAND_WRITE:
		if (pDevice)
			CmdSuccess = pDevice->Write(_Address);
		else
		{
			Memory::Write_U32(FS_EINVAL, _Address + 4);
			CmdSuccess = true;
		}
        break;

    case COMMAND_SEEK:
		if (pDevice)
			CmdSuccess = pDevice->Seek(_Address);
		else
		{
			Memory::Write_U32(FS_EINVAL, _Address + 4);
			CmdSuccess = true;
		}
        break;

    case COMMAND_IOCTL:
		if (pDevice)
			CmdSuccess = pDevice->IOCtl(_Address);
        break;

    case COMMAND_IOCTLV:
		if (pDevice)
			CmdSuccess = pDevice->IOCtlV(_Address);
        break;

    default:
        _dbg_assert_msg_(WII_IPC_HLE, 0, "Unknown IPC Command %i (0x%08x)", Command, _Address);
        break;
    }

    // It seems that the original hardware overwrites the command after it has been
	// executed. We write 8 which is not any valid command, and what IOS does 
	Memory::Write_U32(8, _Address);
	// IOS seems to write back the command that was responded to
	Memory::Write_U32(Command, _Address + 8);

    if (CmdSuccess)
    {
		// Generate a reply to the IPC command
		EnqReply(_Address);
    }
	else
	{
		if (pDevice)
		{
			INFO_LOG(WII_IPC_HLE, "<<-- Reply Failed to %s IPC Request %i @ 0x%08x ", pDevice->GetDeviceName().c_str(), Command, _Address);
		}
		else
		{
			INFO_LOG(WII_IPC_HLE, "<<-- Reply Failed to Unknown (%08x) IPC Request %i @ 0x%08x ", DeviceID, Command, _Address);
		}
	}
}

// Happens AS SOON AS IPC gets a new pointer!
void EnqRequest(u32 _Address)
{
	request_queue.push_back(_Address);
}

// Called when IOS module has some reply
void EnqReply(u32 _Address)
{
	reply_queue.push_back(_Address);
}

// This is called every IPC_HLE_PERIOD from SystemTimers.cpp
// Takes care of routing ipc <-> ipc HLE
void Update()
{
	if (!WII_IPCInterface::IsReady())
		return;

	UpdateDevices();

	if (request_queue.size())
	{
		WII_IPCInterface::GenerateAck(request_queue.front());
		INFO_LOG(WII_IPC_HLE, "||-- Acknowledge IPC Request @ 0x%08x", request_queue.front());

		ExecuteCommand(request_queue.front());
		request_queue.pop_front();

#if MAX_LOGLEVEL >= DEBUG_LEVEL
		Dolphin_Debugger::PrintCallstack(LogTypes::WII_IPC_HLE, LogTypes::LDEBUG);
#endif
	}

	if (reply_queue.size())
	{
		WII_IPCInterface::GenerateReply(reply_queue.front());
		INFO_LOG(WII_IPC_HLE, "<<-- Reply to IPC Request @ 0x%08x", reply_queue.front());
		reply_queue.pop_front();
	}
}

void UpdateDevices()
{
	// Check if a hardware device must be updated
	TDeviceMap::const_iterator itrEnd = g_DeviceMap.lower_bound(IPC_FIRST_FILEIO_ID);
	for (TDeviceMap::const_iterator itr = g_DeviceMap.begin(); itr != itrEnd; ++itr) {
		if (itr->second->IsOpened() && itr->second->Update()) {
			break;
		}
	}
}


} // end of namespace WII_IPC_HLE_Interface
