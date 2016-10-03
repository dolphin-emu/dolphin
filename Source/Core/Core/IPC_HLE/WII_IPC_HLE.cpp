// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
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
#include <mutex>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/WII_IPC.h"

#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_DI.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_FileIO.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_es.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_fs.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_net.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_net_ssl.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_sdio_slot0.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_stm.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_emu.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_real.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_kbd.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_ven.h"

#if defined(__LIBUSB__)
#include "Core/IPC_HLE/WII_IPC_HLE_Device_hid.h"
#endif

#include "Core/PowerPC/PowerPC.h"

namespace WII_IPC_HLE_Interface
{
static std::map<u32, std::shared_ptr<IWII_IPC_HLE_Device>> s_device_map;
static std::mutex s_device_map_mutex;

// STATE_TO_SAVE
#define IPC_MAX_FDS 0x18
#define ES_MAX_COUNT 2
static std::shared_ptr<IWII_IPC_HLE_Device> s_fdmap[IPC_MAX_FDS];
static bool s_es_inuse[ES_MAX_COUNT];
static std::shared_ptr<IWII_IPC_HLE_Device> s_es_handles[ES_MAX_COUNT];

using IPCMsgQueue = std::deque<u32>;
static IPCMsgQueue s_request_queue;  // ppc -> arm
static IPCMsgQueue s_reply_queue;    // arm -> ppc
static IPCMsgQueue s_ack_queue;      // arm -> ppc

static CoreTiming::EventType* s_event_enqueue;
static CoreTiming::EventType* s_event_sdio_notify;

static u64 s_last_reply_time;

static constexpr u64 ENQUEUE_REQUEST_FLAG = 0x100000000ULL;
static constexpr u64 ENQUEUE_ACKNOWLEDGEMENT_FLAG = 0x200000000ULL;
static void EnqueueEvent(u64 userdata, s64 cycles_late = 0)
{
  if (userdata & ENQUEUE_ACKNOWLEDGEMENT_FLAG)
  {
    s_ack_queue.push_back(static_cast<u32>(userdata));
  }
  else if (userdata & ENQUEUE_REQUEST_FLAG)
  {
    s_request_queue.push_back(static_cast<u32>(userdata));
  }
  else
  {
    s_reply_queue.push_back(static_cast<u32>(userdata));
  }
  Update();
}

static void SDIO_EventNotify_CPUThread(u64 userdata, s64 cycles_late)
{
  auto device =
      static_cast<CWII_IPC_HLE_Device_sdio_slot0*>(GetDeviceByName("/dev/sdio/slot0").get());
  if (device)
    device->EventNotify();
}

static u32 num_devices;

template <typename T>
std::shared_ptr<T> AddDevice(const char* device_name)
{
  auto device = std::make_shared<T>(num_devices, device_name);
  s_device_map[num_devices] = device;
  num_devices++;
  return device;
}

void Reinit()
{
  std::lock_guard<std::mutex> lock(s_device_map_mutex);
  _assert_msg_(WII_IPC_HLE, s_device_map.empty(), "Reinit called while already initialized");
  CWII_IPC_HLE_Device_es::m_ContentFile = "";

  num_devices = 0;

  // Build hardware devices
  if (!SConfig::GetInstance().m_bt_passthrough_enabled)
    AddDevice<CWII_IPC_HLE_Device_usb_oh1_57e_305_emu>("/dev/usb/oh1/57e/305");
  else
    AddDevice<CWII_IPC_HLE_Device_usb_oh1_57e_305_real>("/dev/usb/oh1/57e/305");

  AddDevice<CWII_IPC_HLE_Device_stm_immediate>("/dev/stm/immediate");
  AddDevice<CWII_IPC_HLE_Device_stm_eventhook>("/dev/stm/eventhook");
  AddDevice<CWII_IPC_HLE_Device_fs>("/dev/fs");

  // IOS allows two ES devices at a time
  for (u32 j = 0; j < ES_MAX_COUNT; j++)
  {
    s_es_handles[j] = AddDevice<CWII_IPC_HLE_Device_es>("/dev/es");
    s_es_inuse[j] = false;
  }

  AddDevice<CWII_IPC_HLE_Device_di>("/dev/di");
  AddDevice<CWII_IPC_HLE_Device_net_kd_request>("/dev/net/kd/request");
  AddDevice<CWII_IPC_HLE_Device_net_kd_time>("/dev/net/kd/time");
  AddDevice<CWII_IPC_HLE_Device_net_ncd_manage>("/dev/net/ncd/manage");
  AddDevice<CWII_IPC_HLE_Device_net_wd_command>("/dev/net/wd/command");
  AddDevice<CWII_IPC_HLE_Device_net_ip_top>("/dev/net/ip/top");
  AddDevice<CWII_IPC_HLE_Device_net_ssl>("/dev/net/ssl");
  AddDevice<CWII_IPC_HLE_Device_usb_kbd>("/dev/usb/kbd");
  AddDevice<CWII_IPC_HLE_Device_usb_ven>("/dev/usb/ven");
  AddDevice<CWII_IPC_HLE_Device_sdio_slot0>("/dev/sdio/slot0");
  AddDevice<CWII_IPC_HLE_Device_stub>("/dev/sdio/slot1");
#if defined(__LIBUSB__)
  AddDevice<CWII_IPC_HLE_Device_hid>("/dev/usb/hid");
#else
  AddDevice<CWII_IPC_HLE_Device_stub>("/dev/usb/hid");
#endif
  AddDevice<CWII_IPC_HLE_Device_stub>("/dev/usb/oh1");
  AddDevice<IWII_IPC_HLE_Device>("_Unimplemented_Device_");
}

void Init()
{
  Reinit();

  s_event_enqueue = CoreTiming::RegisterEvent("IPCEvent", EnqueueEvent);
  s_event_sdio_notify = CoreTiming::RegisterEvent("SDIO_EventNotify", SDIO_EventNotify_CPUThread);
}

void Reset(bool hard)
{
  CoreTiming::RemoveAllEvents(s_event_enqueue);

  for (auto& dev : s_fdmap)
  {
    if (dev && !dev->IsHardware())
    {
      // close all files and delete their resources
      dev->Close(0, true);
    }

    dev.reset();
  }

  for (bool& in_use : s_es_inuse)
  {
    in_use = false;
  }

  {
    std::lock_guard<std::mutex> lock(s_device_map_mutex);
    for (const auto& entry : s_device_map)
    {
      if (entry.second)
      {
        // Force close
        entry.second->Close(0, true);
      }
    }

    if (hard)
      s_device_map.clear();
  }

  s_request_queue.clear();
  s_reply_queue.clear();

  s_last_reply_time = 0;
}

void Shutdown()
{
  Reset(true);
}

void SetDefaultContentFile(const std::string& file_name)
{
  std::lock_guard<std::mutex> lock(s_device_map_mutex);
  for (const auto& entry : s_device_map)
  {
    if (entry.second && entry.second->GetDeviceName().find("/dev/es") == 0)
    {
      static_cast<CWII_IPC_HLE_Device_es*>(entry.second.get())->LoadWAD(file_name);
    }
  }
}

void ES_DIVerify(const std::vector<u8>& tmd)
{
  CWII_IPC_HLE_Device_es::ES_DIVerify(tmd);
}

void SDIO_EventNotify()
{
  // TODO: Potential race condition: If IsRunning() becomes false after
  // it's checked, an event may be scheduled after CoreTiming shuts down.
  if (SConfig::GetInstance().bWii && Core::IsRunning())
    CoreTiming::ScheduleEvent(0, s_event_sdio_notify, 0, CoreTiming::FromThread::NON_CPU);
}

static int GetFreeDeviceID()
{
  for (u32 i = 0; i < IPC_MAX_FDS; i++)
  {
    if (s_fdmap[i] == nullptr)
    {
      return i;
    }
  }

  return -1;
}

std::shared_ptr<IWII_IPC_HLE_Device> GetDeviceByName(const std::string& device_name)
{
  std::lock_guard<std::mutex> lock(s_device_map_mutex);
  for (const auto& entry : s_device_map)
  {
    if (entry.second && entry.second->GetDeviceName() == device_name)
    {
      return entry.second;
    }
  }

  return nullptr;
}

std::shared_ptr<IWII_IPC_HLE_Device> AccessDeviceByID(u32 id)
{
  std::lock_guard<std::mutex> lock(s_device_map_mutex);
  if (s_device_map.find(id) != s_device_map.end())
  {
    return s_device_map[id];
  }

  return nullptr;
}

// This is called from ExecuteCommand() COMMAND_OPEN_DEVICE
std::shared_ptr<IWII_IPC_HLE_Device> CreateFileIO(u32 device_id, const std::string& device_name)
{
  // scan device name and create the right one
  INFO_LOG(WII_IPC_FILEIO, "IOP: Create FileIO %s", device_name.c_str());
  return std::make_shared<CWII_IPC_HLE_Device_FileIO>(device_id, device_name);
}

void DoState(PointerWrap& p)
{
  p.Do(s_request_queue);
  p.Do(s_reply_queue);
  p.Do(s_last_reply_time);

  // We need to make sure all file handles are closed so WII_IPC_Devices_fs::DoState can
  // successfully save or re-create /tmp
  for (auto& descriptor : s_fdmap)
  {
    if (descriptor)
      descriptor->PrepareForState(p.GetMode());
  }

  for (const auto& entry : s_device_map)
  {
    if (entry.second->IsHardware())
    {
      entry.second->DoState(p);
    }
  }

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    for (u32 i = 0; i < IPC_MAX_FDS; i++)
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
          s_fdmap[i] = AccessDeviceByID(hwId);
        }
        else
        {
          s_fdmap[i] = std::make_shared<CWII_IPC_HLE_Device_FileIO>(i, "");
          s_fdmap[i]->DoState(p);
        }
      }
    }

    for (u32 i = 0; i < ES_MAX_COUNT; i++)
    {
      p.Do(s_es_inuse[i]);
      u32 handleID = s_es_handles[i]->GetDeviceID();
      p.Do(handleID);

      s_es_handles[i] = AccessDeviceByID(handleID);
    }
  }
  else
  {
    for (auto& descriptor : s_fdmap)
    {
      u32 exists = descriptor ? 1 : 0;
      p.Do(exists);
      if (exists)
      {
        u32 isHw = descriptor->IsHardware() ? 1 : 0;
        p.Do(isHw);
        if (isHw)
        {
          u32 hwId = descriptor->GetDeviceID();
          p.Do(hwId);
        }
        else
        {
          descriptor->DoState(p);
        }
      }
    }

    for (u32 i = 0; i < ES_MAX_COUNT; i++)
    {
      p.Do(s_es_inuse[i]);
      u32 handleID = s_es_handles[i]->GetDeviceID();
      p.Do(handleID);
    }
  }
}

void ExecuteCommand(u32 address)
{
  IPCCommandResult result = IWII_IPC_HLE_Device::GetNoReply();

  IPCCommandType Command = static_cast<IPCCommandType>(Memory::Read_U32(address));
  s32 DeviceID = Memory::Read_U32(address + 8);

  std::shared_ptr<IWII_IPC_HLE_Device> device =
      (DeviceID >= 0 && DeviceID < IPC_MAX_FDS) ? s_fdmap[DeviceID] : nullptr;

  DEBUG_LOG(WII_IPC_HLE, "-->> Execute Command Address: 0x%08x (code: %x, device: %x) %p", address,
            Command, DeviceID, device.get());

  switch (Command)
  {
  case IPC_CMD_OPEN:
  {
    u32 Mode = Memory::Read_U32(address + 0x10);
    DeviceID = GetFreeDeviceID();

    std::string device_name = Memory::GetString(Memory::Read_U32(address + 0xC));

    INFO_LOG(WII_IPC_HLE, "Trying to open %s as %d", device_name.c_str(), DeviceID);
    if (DeviceID >= 0)
    {
      if (device_name.find("/dev/es") == 0)
      {
        u32 j;
        for (j = 0; j < ES_MAX_COUNT; j++)
        {
          if (!s_es_inuse[j])
          {
            s_es_inuse[j] = true;
            s_fdmap[DeviceID] = s_es_handles[j];
            result = s_es_handles[j]->Open(address, Mode);
            Memory::Write_U32(DeviceID, address + 4);
            break;
          }
        }

        if (j == ES_MAX_COUNT)
        {
          Memory::Write_U32(FS_EESEXHAUSTED, address + 4);
          result = IWII_IPC_HLE_Device::GetDefaultReply();
        }
      }
      else if (device_name.find("/dev/") == 0)
      {
        device = GetDeviceByName(device_name);
        if (device)
        {
          s_fdmap[DeviceID] = device;
          result = device->Open(address, Mode);
          DEBUG_LOG(WII_IPC_FILEIO, "IOP: ReOpen (Device=%s, DeviceID=%08x, Mode=%i)",
                    device->GetDeviceName().c_str(), DeviceID, Mode);
          Memory::Write_U32(DeviceID, address + 4);
        }
        else
        {
          WARN_LOG(WII_IPC_HLE, "Unimplemented device: %s", device_name.c_str());
          Memory::Write_U32(FS_ENOENT, address + 4);
          result = IWII_IPC_HLE_Device::GetDefaultReply();
        }
      }
      else
      {
        device = CreateFileIO(DeviceID, device_name);
        result = device->Open(address, Mode);

        DEBUG_LOG(WII_IPC_FILEIO, "IOP: Open File (Device=%s, ID=%08x, Mode=%i)",
                  device->GetDeviceName().c_str(), DeviceID, Mode);
        if (Memory::Read_U32(address + 4) == (u32)DeviceID)
        {
          s_fdmap[DeviceID] = device;
        }
      }
    }
    else
    {
      Memory::Write_U32(FS_EFDEXHAUSTED, address + 4);
      result = IWII_IPC_HLE_Device::GetDefaultReply();
    }
    break;
  }
  case IPC_CMD_CLOSE:
  {
    if (device)
    {
      result = device->Close(address);

      for (u32 j = 0; j < ES_MAX_COUNT; j++)
      {
        if (s_es_handles[j] == s_fdmap[DeviceID])
        {
          s_es_inuse[j] = false;
        }
      }

      s_fdmap[DeviceID].reset();
    }
    else
    {
      Memory::Write_U32(FS_EINVAL, address + 4);
      result = IWII_IPC_HLE_Device::GetDefaultReply();
    }
    break;
  }
  case IPC_CMD_READ:
  {
    if (device)
    {
      result = device->Read(address);
    }
    else
    {
      Memory::Write_U32(FS_EINVAL, address + 4);
      result = IWII_IPC_HLE_Device::GetDefaultReply();
    }
    break;
  }
  case IPC_CMD_WRITE:
  {
    if (device)
    {
      result = device->Write(address);
    }
    else
    {
      Memory::Write_U32(FS_EINVAL, address + 4);
      result = IWII_IPC_HLE_Device::GetDefaultReply();
    }
    break;
  }
  case IPC_CMD_SEEK:
  {
    if (device)
    {
      result = device->Seek(address);
    }
    else
    {
      Memory::Write_U32(FS_EINVAL, address + 4);
      result = IWII_IPC_HLE_Device::GetDefaultReply();
    }
    break;
  }
  case IPC_CMD_IOCTL:
  {
    if (device)
    {
      result = device->IOCtl(address);
    }
    break;
  }
  case IPC_CMD_IOCTLV:
  {
    if (device)
    {
      result = device->IOCtlV(address);
    }
    break;
  }
  default:
  {
    _dbg_assert_msg_(WII_IPC_HLE, 0, "Unknown IPC Command %i (0x%08x)", Command, address);
    break;
  }
  }

  // Ensure replies happen in order
  const s64 ticks_until_last_reply = s_last_reply_time - CoreTiming::GetTicks();
  if (ticks_until_last_reply > 0)
    result.reply_delay_ticks += ticks_until_last_reply;
  s_last_reply_time = CoreTiming::GetTicks() + result.reply_delay_ticks;

  if (result.send_reply)
  {
    // The original hardware overwrites the command type with the async reply type.
    Memory::Write_U32(IPC_REP_ASYNC, address);
    // IOS also seems to write back the command that was responded to in the FD field.
    Memory::Write_U32(Command, address + 8);
    // Generate a reply to the IPC command
    EnqueueReply(address, (int)result.reply_delay_ticks);
  }
}

// Happens AS SOON AS IPC gets a new pointer!
void EnqueueRequest(u32 address)
{
  CoreTiming::ScheduleEvent(1000, s_event_enqueue, address | ENQUEUE_REQUEST_FLAG);
}

// Called when IOS module has some reply
// NOTE: Only call this if you have correctly handled
//       CommandAddress+0 and CommandAddress+8.
//       Please search for examples of this being called elsewhere.
void EnqueueReply(u32 address, int cycles_in_future, CoreTiming::FromThread from)
{
  CoreTiming::ScheduleEvent(cycles_in_future, s_event_enqueue, address, from);
}

void EnqueueCommandAcknowledgement(u32 address, int cycles_in_future)
{
  CoreTiming::ScheduleEvent(cycles_in_future, s_event_enqueue,
                            address | ENQUEUE_ACKNOWLEDGEMENT_FLAG);
}

// This is called every IPC_HLE_PERIOD from SystemTimers.cpp
// Takes care of routing ipc <-> ipc HLE
void Update()
{
  if (!WII_IPCInterface::IsReady())
    return;

  if (s_request_queue.size())
  {
    WII_IPCInterface::GenerateAck(s_request_queue.front());
    DEBUG_LOG(WII_IPC_HLE, "||-- Acknowledge IPC Request @ 0x%08x", s_request_queue.front());
    u32 command = s_request_queue.front();
    s_request_queue.pop_front();
    ExecuteCommand(command);
    return;
  }

  if (s_reply_queue.size())
  {
    WII_IPCInterface::GenerateReply(s_reply_queue.front());
    DEBUG_LOG(WII_IPC_HLE, "<<-- Reply to IPC Request @ 0x%08x", s_reply_queue.front());
    s_reply_queue.pop_front();
    return;
  }

  if (s_ack_queue.size())
  {
    WII_IPCInterface::GenerateAck(s_ack_queue.front());
    WARN_LOG(WII_IPC_HLE, "<<-- Double-ack to IPC Request @ 0x%08x", s_ack_queue.front());
    s_ack_queue.pop_front();
    return;
  }
}

void UpdateDevices()
{
  // Check if a hardware device must be updated
  for (const auto& entry : s_device_map)
  {
    if (entry.second->IsOpened())
    {
      entry.second->Update();
    }
  }
}

}  // end of namespace WII_IPC_HLE_Interface

// TODO: create WII_IPC_HLE_Device.cpp ?
void IWII_IPC_HLE_Device::DoStateShared(PointerWrap& p)
{
  p.Do(m_Name);
  p.Do(m_DeviceID);
  p.Do(m_Hardware);
  p.Do(m_Active);
}
