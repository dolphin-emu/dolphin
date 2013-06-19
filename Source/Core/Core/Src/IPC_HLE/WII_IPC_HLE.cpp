// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
#include "WII_IPC_HLE_Device_sdio_slot0.h"

#include "FileUtil.h" // For Copy
#include "../ConfigManager.h"
#include "../HW/CPU.h"
#include "../HW/Memmap.h"
#include "../HW/WII_IPC.h"
#include "../Debugger/Debugger_SymbolMap.h"
#include "../PowerPC/PowerPC.h"
#include "../HW/SystemTimers.h"
#include "CoreTiming.h"


namespace WII_IPC_HLE_Interface
{

typedef std::map<u32, IWII_IPC_HLE_Device*> TDeviceMap;
TDeviceMap g_DeviceMap;

// STATE_TO_SAVE
typedef std::map<u32, std::string> TFileNameMap;

#define IPC_MAX_FDS 0x18
#define ES_MAX_COUNT 2
IWII_IPC_HLE_Device* g_FdMap[IPC_MAX_FDS];
bool es_inuse[ES_MAX_COUNT];
IWII_IPC_HLE_Device* es_handles[ES_MAX_COUNT];


typedef std::deque<u32> ipc_msg_queue;
static ipc_msg_queue request_queue;	// ppc -> arm
static ipc_msg_queue reply_queue;	// arm -> ppc

static int enque_reply;

static u64 last_reply_time;

void EnqueReplyCallback(u64 userdata, int)
{
	reply_queue.push_back(userdata);
}

void Init()
{
	
	_dbg_assert_msg_(WII_IPC_HLE, g_DeviceMap.empty(), "DeviceMap isn't empty on init");
	CWII_IPC_HLE_Device_es::m_ContentFile = "";
	u32 i;
	for (i=0; i<IPC_MAX_FDS; i++)
	{
		g_FdMap[i] = NULL;
	}

	i = 0;
	// Build hardware devices
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_oh1_57e_305(i, std::string("/dev/usb/oh1/57e/305")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stm_immediate(i, std::string("/dev/stm/immediate")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stm_eventhook(i, std::string("/dev/stm/eventhook")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_fs(i, std::string("/dev/fs")); i++;

	// IOS allows two ES devices at a time<
	u32 j;
	for (j=0; j<ES_MAX_COUNT; j++)
	{
		g_DeviceMap[i] = es_handles[j] = new CWII_IPC_HLE_Device_es(i, std::string("/dev/es")); i++;
		es_inuse[j] = false;
	}

	g_DeviceMap[i] = new CWII_IPC_HLE_Device_di(i, std::string("/dev/di")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_kd_request(i, std::string("/dev/net/kd/request")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_kd_time(i, std::string("/dev/net/kd/time")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_ncd_manage(i, std::string("/dev/net/ncd/manage")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_ip_top(i, std::string("/dev/net/ip/top")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_kbd(i, std::string("/dev/usb/kbd")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_sdio_slot0(i, std::string("/dev/sdio/slot0")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stub(i, std::string("/dev/sdio/slot1")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stub(i, std::string("/dev/usb/hid")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stub(i, std::string("/dev/usb/oh1")); i++;
	g_DeviceMap[i] = new IWII_IPC_HLE_Device(i, std::string("_Unimplemented_Device_")); i++;
	
	enque_reply = CoreTiming::RegisterEvent("IPCReply", EnqueReplyCallback);
}

void Reset(bool _bHard)
{
	
	CoreTiming::RemoveAllEvents(enque_reply);
	
	u32 i;
	for (i=0; i<IPC_MAX_FDS; i++)
	{
		if (g_FdMap[i] != NULL && !g_FdMap[i]->IsHardware())
		{
			// close all files and delete their resources
			g_FdMap[i]->Close(0, true);
			delete g_FdMap[i];
		}
		g_FdMap[i] = NULL;
	}

	TDeviceMap::iterator itr = g_DeviceMap.begin();
	while (itr != g_DeviceMap.end())
	{
		if (itr->second)
		{
			// Force close
			itr->second->Close(0, true);
			// Hardware should not be deleted unless it is a hard reset
			if (_bHard)
				delete itr->second;
		}
		++itr;
	}
	if (_bHard)
	{
		g_DeviceMap.erase(g_DeviceMap.begin(), g_DeviceMap.end());
	}
	request_queue.clear();
	reply_queue.clear();
	last_reply_time = 0;
}

void Shutdown()
{
	Reset(true);
}

void SetDefaultContentFile(const std::string& _rFilename)
{
	TDeviceMap::const_iterator itr = g_DeviceMap.begin();
	while (itr != g_DeviceMap.end())
	{
		if (itr->second && itr->second->GetDeviceName().find(std::string("/dev/es")) == 0)
		{
			((CWII_IPC_HLE_Device_es*)itr->second)->LoadWAD(_rFilename);
		}
		++itr;
	}
}

void ES_DIVerify(u8 *_pTMD, u32 _sz)
{
	CWII_IPC_HLE_Device_es::ES_DIVerify(_pTMD, _sz);
}

void SDIO_EventNotify()
{
	CWII_IPC_HLE_Device_sdio_slot0 *pDevice =
		(CWII_IPC_HLE_Device_sdio_slot0*)GetDeviceByName(std::string("/dev/sdio/slot0"));
	if (pDevice)
		pDevice->EventNotify();
}
int getFreeDeviceId()
{
	u32 i;
	for (i=0; i<IPC_MAX_FDS; i++)
	{
		if (g_FdMap[i] == NULL)
		{
			return i;
		}
	}
	return -1;
}

IWII_IPC_HLE_Device* GetDeviceByName(const std::string& _rDeviceName)
{
	TDeviceMap::const_iterator itr = g_DeviceMap.begin();
	while (itr != g_DeviceMap.end())
	{
		if (itr->second && itr->second->GetDeviceName() == _rDeviceName)
			return itr->second;
		++itr;
	}

	return NULL;
}

IWII_IPC_HLE_Device* AccessDeviceByID(u32 _ID)
{
	if (g_DeviceMap.find(_ID) != g_DeviceMap.end())
		return g_DeviceMap[_ID];

		return NULL;
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
	p.Do(request_queue);
	p.Do(reply_queue);
	p.Do(last_reply_time);

	TDeviceMap::const_iterator itr;

	itr = g_DeviceMap.begin();
	while (itr != g_DeviceMap.end())
	{
			if (itr->second->IsHardware())
			{
				itr->second->DoState(p);
			}
			++itr;
	}

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		u32 i;
		for (i=0; i<IPC_MAX_FDS; i++)
		{
			u32 exists = 0;
			p.Do(exists);
			if (exists)
			{
				u32 isHw = 0;
				p.Do(isHw);
				if (isHw)
				{
					u32 hwId = 0;
					p.Do(hwId);
					g_FdMap[i] = AccessDeviceByID(hwId);
				}
				else
				{
					g_FdMap[i] = new CWII_IPC_HLE_Device_FileIO(i, "");
					g_FdMap[i]->DoState(p);
				}
			}
			else
			{
				g_FdMap[i] = NULL;
			}
		}
		for (i=0; i<ES_MAX_COUNT; i++)
		{
			p.Do(es_inuse[i]);
			u32 handleID = es_handles[i]->GetDeviceID();
			p.Do(handleID);

			es_handles[i] = AccessDeviceByID(handleID);
		}
	}
	else
	{
		u32 i;
		for (i=0; i<IPC_MAX_FDS; i++)
		{
			u32 exists = g_FdMap[i] ? 1 : 0;
			p.Do(exists);
			if (exists)
			{
				u32 isHw = g_FdMap[i]->IsHardware() ? 1 : 0;
				p.Do(isHw);
				if (isHw)
				{
					u32 hwId = g_FdMap[i]->GetDeviceID();
					p.Do(hwId);
				}
				else
				{
					g_FdMap[i]->DoState(p);
				}
			}
		}
		for (i=0; i<ES_MAX_COUNT; i++)
		{
			p.Do(es_inuse[i]);
			u32 handleID = es_handles[i]->GetDeviceID();
			p.Do(handleID);
		}
	}
}

void ExecuteCommand(u32 _Address)
{
	bool CmdSuccess = false;

	ECommandType Command = static_cast<ECommandType>(Memory::Read_U32(_Address));
	volatile s32 DeviceID = Memory::Read_U32(_Address + 8);

	IWII_IPC_HLE_Device* pDevice = (DeviceID >= 0 && DeviceID < IPC_MAX_FDS) ? g_FdMap[DeviceID] : NULL;

	INFO_LOG(WII_IPC_HLE, "-->> Execute Command Address: 0x%08x (code: %x, device: %x) %p", _Address, Command, DeviceID, pDevice);

	switch (Command)
	{
	case COMMAND_OPEN_DEVICE:
	{
		u32 Mode = Memory::Read_U32(_Address + 0x10);
		DeviceID = getFreeDeviceId();
		
		std::string DeviceName;
		Memory::GetString(DeviceName, Memory::Read_U32(_Address + 0xC));

		
		WARN_LOG(WII_IPC_HLE, "Trying to open %s as %d", DeviceName.c_str(), DeviceID);
		if (DeviceID >= 0)
		{
			if (DeviceName.find("/dev/es") == 0)
			{
				u32 j;
				for (j=0; j<ES_MAX_COUNT; j++)
				{
					if (!es_inuse[j])
					{
						es_inuse[j] = true;
						g_FdMap[DeviceID] = es_handles[j];
						CmdSuccess = es_handles[j]->Open(_Address, Mode);
						Memory::Write_U32(DeviceID, _Address+4);
						break;
					}
				}
				if (j == ES_MAX_COUNT)
				{
					Memory::Write_U32(FS_EESEXHAUSTED, _Address + 4);
					CmdSuccess = true;
				}

			}
			else if (DeviceName.find("/dev/") == 0)
			{
				pDevice = GetDeviceByName(DeviceName);
				if (pDevice)
				{
					g_FdMap[DeviceID] = pDevice;
					CmdSuccess = pDevice->Open(_Address, Mode);
					INFO_LOG(WII_IPC_FILEIO, "IOP: ReOpen (Device=%s, DeviceID=%08x, Mode=%i)",
						pDevice->GetDeviceName().c_str(), DeviceID, Mode);
					Memory::Write_U32(DeviceID, _Address+4);
				}
				else
				{
					WARN_LOG(WII_IPC_HLE, "Unimplemented device: %s", DeviceName.c_str());
					Memory::Write_U32(FS_ENOENT, _Address+4);
					CmdSuccess = true;
				}
			}
			else
			{
				pDevice = CreateFileIO(DeviceID, DeviceName);
				CmdSuccess = pDevice->Open(_Address, Mode);

				INFO_LOG(WII_IPC_FILEIO, "IOP: Open File (Device=%s, ID=%08x, Mode=%i)",
						pDevice->GetDeviceName().c_str(), DeviceID, Mode);
				if (Memory::Read_U32(_Address + 4) == (u32)DeviceID)
				{
					g_FdMap[DeviceID] = pDevice;
				}
				else
				{
					delete pDevice;
					pDevice = NULL;
				}
			}

		}
		else
		{
			Memory::Write_U32(FS_EFDEXHAUSTED, _Address + 4);
			CmdSuccess = true;
		}
		break;
	}
	case COMMAND_CLOSE_DEVICE:
	{
		if (pDevice)
		{
			CmdSuccess = pDevice->Close(_Address);

			u32 j;
			for (j=0; j<ES_MAX_COUNT; j++)
			{
				if (es_handles[j] == g_FdMap[DeviceID])
				{
					es_inuse[j] = false;
				}
			}

			g_FdMap[DeviceID] = NULL;

			// Don't delete hardware
			if (!pDevice->IsHardware())
			{
				delete pDevice;
				pDevice = NULL;
			}
		}
		else
		{
			Memory::Write_U32(FS_EINVAL, _Address + 4);
			CmdSuccess = true;
		}
		break;
	}
	case COMMAND_READ:
	{
		if (pDevice)
		{
			CmdSuccess = pDevice->Read(_Address);
		}
		else
		{
			Memory::Write_U32(FS_EINVAL, _Address + 4);
			CmdSuccess = true;
		}
		break;
	}
	case COMMAND_WRITE:
	{
		if (pDevice)
		{
			CmdSuccess = pDevice->Write(_Address);
		}
		else
		{
			Memory::Write_U32(FS_EINVAL, _Address + 4);
			CmdSuccess = true;
		}
		break;
	}
	case COMMAND_SEEK:
	{
		if (pDevice)
		{
			CmdSuccess = pDevice->Seek(_Address);
		}
		else
		{
			Memory::Write_U32(FS_EINVAL, _Address + 4);
			CmdSuccess = true;
		}
		break;
	}
	case COMMAND_IOCTL:
	{
		if (pDevice)
		{
			CmdSuccess = pDevice->IOCtl(_Address);
		}
		break;
	}
	case COMMAND_IOCTLV:
	{
		if (pDevice)
		{
			CmdSuccess = pDevice->IOCtlV(_Address);
		}
		break;
	}
	default:
	{
		_dbg_assert_msg_(WII_IPC_HLE, 0, "Unknown IPC Command %i (0x%08x)", Command, _Address);
		break;
	}
	}

	// It seems that the original hardware overwrites the command after it has been
	// executed. We write 8 which is not any valid command, and what IOS does 
	Memory::Write_U32(8, _Address);
	// IOS seems to write back the command that was responded to
	Memory::Write_U32(Command, _Address + 8);

	if (CmdSuccess)
	{
		// Ensure replies happen in order, fairly ugly
		// Without this, tons of games fail now that DI commands have different reply delays
		int reply_delay = pDevice ? pDevice->GetCmdDelay(_Address) : 0;
		
		const s64 ticks_til_last_reply = last_reply_time - CoreTiming::GetTicks();
		
		if (ticks_til_last_reply > 0)
			reply_delay = ticks_til_last_reply;
		
		last_reply_time = CoreTiming::GetTicks() + reply_delay;
	
		// Generate a reply to the IPC command
		EnqReply(_Address, reply_delay);
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
void EnqReply(u32 _Address, int cycles_in_future)
{
	CoreTiming::ScheduleEvent(cycles_in_future, enque_reply, _Address);
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
		u32 command = request_queue.front();
		request_queue.pop_front();
		ExecuteCommand(command);

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
	for (TDeviceMap::const_iterator itr = g_DeviceMap.begin(); itr != g_DeviceMap.end(); ++itr)
	{
		if (itr->second->IsOpened() && itr->second->Update())
		{
			break;
		}
	}
}


} // end of namespace WII_IPC_HLE_Interface

// TODO: create WII_IPC_HLE_Device.cpp ?
void IWII_IPC_HLE_Device::DoStateShared(PointerWrap& p)
{
	p.Do(m_Name);
	p.Do(m_DeviceID);
	p.Do(m_Hardware);
	p.Do(m_Active);
}
