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

#include <list>
#include <map>
#include <string>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/WII_IPC.h"

#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_DI.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_es.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_FileIO.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_fs.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_net.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_net_ssl.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_sdio_slot0.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_stm.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_kbd.h"

#if defined(__LIBUSB__) || defined (_WIN32)
	#include "Core/IPC_HLE/WII_IPC_HLE_Device_hid.h"
#endif

#include "Core/PowerPC/PowerPC.h"


namespace WII_IPC_HLE_Interface
{

typedef std::map<u32, IWII_IPC_HLE_Device*> TDeviceMap;
static TDeviceMap g_DeviceMap;

// STATE_TO_SAVE
typedef std::map<u32, std::string> TFileNameMap;

#define IPC_MAX_FDS 0x18
#define ES_MAX_COUNT 2
static IWII_IPC_HLE_Device* g_FdMap[IPC_MAX_FDS];
static bool es_inuse[ES_MAX_COUNT];
static IWII_IPC_HLE_Device* es_handles[ES_MAX_COUNT];


typedef std::deque<u32> ipc_msg_queue;
static ipc_msg_queue request_queue; // ppc -> arm
static ipc_msg_queue reply_queue;   // arm -> ppc
static ipc_msg_queue ack_queue;   // arm -> ppc

static int event_enqueue;

static u64 last_reply_time;

static const u64 ENQUEUE_REQUEST_FLAG = 0x100000000ULL;
static const u64 ENQUEUE_ACKNOWLEDGEMENT_FLAG = 0x200000000ULL;
static void EnqueueEventCallback(u64 userdata, int)
{
	if (userdata & ENQUEUE_ACKNOWLEDGEMENT_FLAG)
	{
		ack_queue.push_back((u32)userdata);
	}
	else if (userdata & ENQUEUE_REQUEST_FLAG)
	{
		request_queue.push_back((u32)userdata);
	}
	else
	{
		reply_queue.push_back((u32)userdata);
	}
	Update();
}

void Init()
{
	_dbg_assert_msg_(WII_IPC_HLE, g_DeviceMap.empty(), "DeviceMap isn't empty on init");
	CWII_IPC_HLE_Device_es::m_ContentFile = "";

	for (IWII_IPC_HLE_Device*& dev : g_FdMap)
	{
		dev = nullptr;
	}

	u32 i = 0;
	// Build hardware devices
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_oh1_57e_305(i, "/dev/usb/oh1/57e/305"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stm_immediate(i, "/dev/stm/immediate"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stm_eventhook(i, "/dev/stm/eventhook"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_fs(i, "/dev/fs"); i++;

	// IOS allows two ES devices at a time
	for (u32 j=0; j<ES_MAX_COUNT; j++)
	{
		g_DeviceMap[i] = es_handles[j] = new CWII_IPC_HLE_Device_es(i, "/dev/es"); i++;
		es_inuse[j] = false;
	}

	g_DeviceMap[i] = new CWII_IPC_HLE_Device_di(i, std::string("/dev/di")); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_kd_request(i, "/dev/net/kd/request"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_kd_time(i, "/dev/net/kd/time"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_ncd_manage(i, "/dev/net/ncd/manage"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_wd_command(i, "/dev/net/wd/command"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_ip_top(i, "/dev/net/ip/top"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_net_ssl(i, "/dev/net/ssl"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_usb_kbd(i, "/dev/usb/kbd"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_sdio_slot0(i, "/dev/sdio/slot0"); i++;
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stub(i, "/dev/sdio/slot1"); i++;
	#if defined(__LIBUSB__) || defined(_WIN32)
		g_DeviceMap[i] = new CWII_IPC_HLE_Device_hid(i, "/dev/usb/hid"); i++;
	#else
		g_DeviceMap[i] = new CWII_IPC_HLE_Device_stub(i, "/dev/usb/hid"); i++;
	#endif
	g_DeviceMap[i] = new CWII_IPC_HLE_Device_stub(i, "/dev/usb/oh1"); i++;
	g_DeviceMap[i] = new IWII_IPC_HLE_Device(i, "_Unimplemented_Device_"); i++;

	event_enqueue = CoreTiming::RegisterEvent("IPCEvent", EnqueueEventCallback);
}

void Reset(bool _bHard)
{
	CoreTiming::RemoveAllEvents(event_enqueue);

	for (IWII_IPC_HLE_Device*& dev : g_FdMap)
	{
		if (dev != nullptr && !dev->IsHardware())
		{
			// close all files and delete their resources
			dev->Close(0, true);
			delete dev;
		}

		dev = nullptr;
	}

	for (bool& in_use : es_inuse)
	{
		in_use = false;
	}

	for (const auto& entry : g_DeviceMap)
	{
		if (entry.second)
		{
			// Force close
			entry.second->Close(0, true);

			// Hardware should not be deleted unless it is a hard reset
			if (_bHard)
				delete entry.second;
		}
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
	for (const auto& entry : g_DeviceMap)
	{
		if (entry.second && entry.second->GetDeviceName().find("/dev/es") == 0)
		{
			((CWII_IPC_HLE_Device_es*)entry.second)->LoadWAD(_rFilename);
		}
	}
}

void ES_DIVerify(u8 *_pTMD, u32 _sz)
{
	CWII_IPC_HLE_Device_es::ES_DIVerify(_pTMD, _sz);
}

void SDIO_EventNotify()
{
	CWII_IPC_HLE_Device_sdio_slot0 *pDevice =
		(CWII_IPC_HLE_Device_sdio_slot0*)GetDeviceByName("/dev/sdio/slot0");
	if (pDevice)
		pDevice->EventNotify();
}

int getFreeDeviceId()
{
	for (u32 i=0; i<IPC_MAX_FDS; i++)
	{
		if (g_FdMap[i] == nullptr)
		{
			return i;
		}
	}

	return -1;
}

IWII_IPC_HLE_Device* GetDeviceByName(const std::string& _rDeviceName)
{
	for (const auto& entry : g_DeviceMap)
	{
		if (entry.second && entry.second->GetDeviceName() == _rDeviceName)
		{
			return entry.second;
		}
	}

	return nullptr;
}

IWII_IPC_HLE_Device* AccessDeviceByID(u32 _ID)
{
	if (g_DeviceMap.find(_ID) != g_DeviceMap.end())
	{
		return g_DeviceMap[_ID];
	}

	return nullptr;
}

// This is called from ExecuteCommand() COMMAND_OPEN_DEVICE
IWII_IPC_HLE_Device* CreateFileIO(u32 _DeviceID, const std::string& _rDeviceName)
{
	// scan device name and create the right one
	INFO_LOG(WII_IPC_FILEIO, "IOP: Create FileIO %s", _rDeviceName.c_str());
	return new CWII_IPC_HLE_Device_FileIO(_DeviceID, _rDeviceName);
}


void DoState(PointerWrap &p)
{
	p.Do(request_queue);
	p.Do(reply_queue);
	p.Do(last_reply_time);

	for (const auto& entry : g_DeviceMap)
	{
		if (entry.second->IsHardware())
		{
			entry.second->DoState(p);
		}
	}

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		for (u32 i=0; i<IPC_MAX_FDS; i++)
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
				g_FdMap[i] = nullptr;
			}
		}

		for (u32 i=0; i<ES_MAX_COUNT; i++)
		{
			p.Do(es_inuse[i]);
			u32 handleID = es_handles[i]->GetDeviceID();
			p.Do(handleID);

			es_handles[i] = AccessDeviceByID(handleID);
		}
	}
	else
	{
		for (IWII_IPC_HLE_Device*& dev : g_FdMap)
		{
			u32 exists = dev ? 1 : 0;
			p.Do(exists);
			if (exists)
			{
				u32 isHw = dev->IsHardware() ? 1 : 0;
				p.Do(isHw);
				if (isHw)
				{
					u32 hwId = dev->GetDeviceID();
					p.Do(hwId);
				}
				else
				{
					dev->DoState(p);
				}
			}
		}

		for (u32 i=0; i<ES_MAX_COUNT; i++)
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

	IPCCommandType Command = static_cast<IPCCommandType>(Memory::Read_U32(_Address));
	volatile s32 DeviceID = Memory::Read_U32(_Address + 8);

	IWII_IPC_HLE_Device* pDevice = (DeviceID >= 0 && DeviceID < IPC_MAX_FDS) ? g_FdMap[DeviceID] : nullptr;

	INFO_LOG(WII_IPC_HLE, "-->> Execute Command Address: 0x%08x (code: %x, device: %x) %p", _Address, Command, DeviceID, pDevice);

	switch (Command)
	{
	case IPC_CMD_OPEN:
	{
		u32 Mode = Memory::Read_U32(_Address + 0x10);
		DeviceID = getFreeDeviceId();

		std::string DeviceName = Memory::GetString(Memory::Read_U32(_Address + 0xC));

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
					pDevice = nullptr;
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
	case IPC_CMD_CLOSE:
	{
		if (pDevice)
		{
			CmdSuccess = pDevice->Close(_Address);

			for (u32 j=0; j<ES_MAX_COUNT; j++)
			{
				if (es_handles[j] == g_FdMap[DeviceID])
				{
					es_inuse[j] = false;
				}
			}

			g_FdMap[DeviceID] = nullptr;

			// Don't delete hardware
			if (!pDevice->IsHardware())
			{
				delete pDevice;
				pDevice = nullptr;
			}
		}
		else
		{
			Memory::Write_U32(FS_EINVAL, _Address + 4);
			CmdSuccess = true;
		}
		break;
	}
	case IPC_CMD_READ:
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
	case IPC_CMD_WRITE:
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
	case IPC_CMD_SEEK:
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
	case IPC_CMD_IOCTL:
	{
		if (pDevice)
		{
			CmdSuccess = pDevice->IOCtl(_Address);
		}
		break;
	}
	case IPC_CMD_IOCTLV:
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


	if (CmdSuccess)
	{
		// The original hardware overwrites the command type with the async reply type.
		Memory::Write_U32(IPC_REP_ASYNC, _Address);
		// IOS also seems to write back the command that was responded to in the FD field.
		Memory::Write_U32(Command, _Address + 8);

		// Ensure replies happen in order, fairly ugly
		// Without this, tons of games fail now that DI commands have different reply delays
		int reply_delay = pDevice ? pDevice->GetCmdDelay(_Address) : 0;
		if (!reply_delay)
		{
			int delay_us = 250;
			reply_delay = SystemTimers::GetTicksPerSecond() / 1000000 * delay_us;
		}

		const s64 ticks_til_last_reply = last_reply_time - CoreTiming::GetTicks();

		if (ticks_til_last_reply > 0)
		{
			reply_delay = (int)ticks_til_last_reply;
		}

		last_reply_time = CoreTiming::GetTicks() + reply_delay;

		// Generate a reply to the IPC command
		EnqueueReply(_Address, reply_delay);
	}
}

// Happens AS SOON AS IPC gets a new pointer!
void EnqueueRequest(u32 address)
{
	CoreTiming::ScheduleEvent(1000, event_enqueue, address | ENQUEUE_REQUEST_FLAG);
}

// Called when IOS module has some reply
// NOTE: Only call this if you have correctly handled
//       CommandAddress+0 and CommandAddress+8.
//       Please search for examples of this being called elsewhere.
void EnqueueReply(u32 address, int cycles_in_future)
{
	CoreTiming::ScheduleEvent(cycles_in_future, event_enqueue, address);
}

void EnqueueReply_Threadsafe(u32 address, int cycles_in_future)
{
	CoreTiming::ScheduleEvent_Threadsafe(cycles_in_future, event_enqueue, address);
}

void EnqueueCommandAcknowledgement(u32 address, int cycles_in_future)
{
	CoreTiming::ScheduleEvent(cycles_in_future, event_enqueue,
	                          address | ENQUEUE_ACKNOWLEDGEMENT_FLAG);
}

// This is called every IPC_HLE_PERIOD from SystemTimers.cpp
// Takes care of routing ipc <-> ipc HLE
void Update()
{
	if (!WII_IPCInterface::IsReady())
		return;

	if (request_queue.size())
	{
		WII_IPCInterface::GenerateAck(request_queue.front());
		INFO_LOG(WII_IPC_HLE, "||-- Acknowledge IPC Request @ 0x%08x", request_queue.front());
		u32 command = request_queue.front();
		request_queue.pop_front();
		ExecuteCommand(command);
		return;
	}

	if (reply_queue.size())
	{
		WII_IPCInterface::GenerateReply(reply_queue.front());
		INFO_LOG(WII_IPC_HLE, "<<-- Reply to IPC Request @ 0x%08x", reply_queue.front());
		reply_queue.pop_front();
		return;
	}

	if (ack_queue.size())
	{
		WII_IPCInterface::GenerateAck(ack_queue.front());
		WARN_LOG(WII_IPC_HLE, "<<-- Double-ack to IPC Request @ 0x%08x", ack_queue.front());
		ack_queue.pop_front();
		return;
	}
}

void UpdateDevices()
{
	// Check if a hardware device must be updated
	for (const auto& entry : g_DeviceMap)
	{
		if (entry.second->IsOpened())
		{
			entry.second->Update();
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
