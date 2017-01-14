// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This is the main Wii IPC file that handles all incoming IPC requests and directs them
// to the right function.
//
// IPC basics (IOS' usage):
// All IPC request handlers will write a return value to 0x04.
//   Open: Device file descriptor or error code
//   Close: IPC_SUCCESS
//   Read: Bytes read
//   Write: Bytes written
//   Seek: Seek position
//   Ioctl: Depends on the handler
//   Ioctlv: Depends on the handler
// Replies may be sent immediately or asynchronously for ioctls and ioctlvs.

#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
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
#include "Core/IPC_HLE/WII_IPC_HLE_Device_stub.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_emu.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_real.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_kbd.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_ven.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_wfssrv.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_wfsi.h"

namespace CoreTiming
{
struct EventType;
}  // namespace CoreTiming

#if defined(__LIBUSB__)
#include "Core/IPC_HLE/WII_IPC_HLE_Device_hid.h"
#endif

namespace WII_IPC_HLE_Interface
{
static std::map<u32, std::shared_ptr<IWII_IPC_HLE_Device>> s_device_map;
static std::mutex s_device_map_mutex;

// STATE_TO_SAVE
constexpr u8 IPC_MAX_FDS = 0x18;
constexpr u8 ES_MAX_COUNT = 3;
static std::shared_ptr<IWII_IPC_HLE_Device> s_fdmap[IPC_MAX_FDS];
static std::shared_ptr<CWII_IPC_HLE_Device_es> s_es_handles[ES_MAX_COUNT];

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
  _assert_(device->GetDeviceType() == IWII_IPC_HLE_Device::DeviceType::Static);
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
  for (auto& es_device : s_es_handles)
    es_device = AddDevice<CWII_IPC_HLE_Device_es>("/dev/es");

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
  AddDevice<CWII_IPC_HLE_Device_usb_wfssrv>("/dev/usb/wfssrv");
  AddDevice<CWII_IPC_HLE_Device_wfsi>("/dev/wfsi");
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

  // Close all devices that were opened and delete their resources
  for (auto& device : s_fdmap)
  {
    if (!device)
      continue;
    device->Close(0, true);
    device.reset();
  }

  if (hard)
  {
    std::lock_guard<std::mutex> lock(s_device_map_mutex);
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
  for (const auto& es : s_es_handles)
    es->LoadWAD(file_name);
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
    entry.second->DoState(p);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    for (u32 i = 0; i < IPC_MAX_FDS; i++)
    {
      u32 exists = 0;
      p.Do(exists);
      if (exists)
      {
        auto device_type = IWII_IPC_HLE_Device::DeviceType::Static;
        p.Do(device_type);
        switch (device_type)
        {
        case IWII_IPC_HLE_Device::DeviceType::Static:
        {
          u32 device_id = 0;
          p.Do(device_id);
          s_fdmap[i] = AccessDeviceByID(device_id);
          break;
        }
        case IWII_IPC_HLE_Device::DeviceType::FileIO:
          s_fdmap[i] = std::make_shared<CWII_IPC_HLE_Device_FileIO>(i, "");
          s_fdmap[i]->DoState(p);
          break;
        }
      }
    }

    for (auto& es_device : s_es_handles)
    {
      const u32 handle_id = es_device->GetDeviceID();
      p.Do(handle_id);
      es_device = std::static_pointer_cast<CWII_IPC_HLE_Device_es>(AccessDeviceByID(handle_id));
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
        auto device_type = descriptor->GetDeviceType();
        p.Do(device_type);
        if (device_type == IWII_IPC_HLE_Device::DeviceType::Static)
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

    for (const auto& es_device : s_es_handles)
    {
      const u32 handle_id = es_device->GetDeviceID();
      p.Do(handle_id);
    }
  }
}

static std::shared_ptr<IWII_IPC_HLE_Device> GetUnusedESDevice()
{
  const auto iterator = std::find_if(std::begin(s_es_handles), std::end(s_es_handles),
                                     [](const auto& es_device) { return !es_device->IsOpened(); });
  return (iterator != std::end(s_es_handles)) ? *iterator : nullptr;
}

// Returns the FD for the newly opened device (on success) or an error code.
static s32 OpenDevice(const u32 address)
{
  const std::string device_name = Memory::GetString(Memory::Read_U32(address + 0xC));
  const u32 open_mode = Memory::Read_U32(address + 0x10);
  const s32 new_fd = GetFreeDeviceID();
  INFO_LOG(WII_IPC_HLE, "Opening %s (mode %d, fd %d)", device_name.c_str(), open_mode, new_fd);
  if (new_fd < 0 || new_fd >= IPC_MAX_FDS)
  {
    ERROR_LOG(WII_IPC_HLE, "Couldn't get a free fd, too many open files");
    return FS_EFDEXHAUSTED;
  }

  std::shared_ptr<IWII_IPC_HLE_Device> device;
  if (device_name.find("/dev/es") == 0)
  {
    device = GetUnusedESDevice();
    if (!device)
      return IPC_EESEXHAUSTED;
  }
  else if (device_name.find("/dev/") == 0)
  {
    device = GetDeviceByName(device_name);
  }
  else if (device_name.find('/') == 0)
  {
    device = std::make_shared<CWII_IPC_HLE_Device_FileIO>(new_fd, device_name);
  }

  if (!device)
  {
    ERROR_LOG(WII_IPC_HLE, "Unknown device: %s", device_name.c_str());
    return IPC_ENOENT;
  }

  Memory::Write_U32(new_fd, address + 4);
  device->Open(address, open_mode);
  const s32 open_return_code = Memory::Read_U32(address + 4);
  if (open_return_code < 0)
    return open_return_code;
  s_fdmap[new_fd] = device;
  return new_fd;
}

static IPCCommandResult HandleCommand(const u32 address)
{
  const auto command = static_cast<IPCCommandType>(Memory::Read_U32(address));
  if (command == IPC_CMD_OPEN)
  {
    const s32 new_fd = OpenDevice(address);
    Memory::Write_U32(new_fd, address + 4);
    return IWII_IPC_HLE_Device::GetDefaultReply();
  }

  const s32 fd = Memory::Read_U32(address + 8);
  const auto device = (fd >= 0 && fd < IPC_MAX_FDS) ? s_fdmap[fd] : nullptr;
  if (!device)
  {
    Memory::Write_U32(IPC_EINVAL, address + 4);
    return IWII_IPC_HLE_Device::GetDefaultReply();
  }

  switch (command)
  {
  case IPC_CMD_CLOSE:
    s_fdmap[fd].reset();
    // A close on a valid device returns IPC_SUCCESS.
    Memory::Write_U32(IPC_SUCCESS, address + 4);
    return device->Close(address);
  case IPC_CMD_READ:
    return device->Read(address);
  case IPC_CMD_WRITE:
    return device->Write(address);
  case IPC_CMD_SEEK:
    return device->Seek(address);
  case IPC_CMD_IOCTL:
    return device->IOCtl(address);
  case IPC_CMD_IOCTLV:
    return device->IOCtlV(address);
  default:
    _assert_msg_(WII_IPC_HLE, false, "Unexpected command: %x", command);
    return IWII_IPC_HLE_Device::GetDefaultReply();
  }
}

void ExecuteCommand(const u32 address)
{
  IPCCommandResult result = HandleCommand(address);

  // Ensure replies happen in order
  const s64 ticks_until_last_reply = s_last_reply_time - CoreTiming::GetTicks();
  if (ticks_until_last_reply > 0)
    result.reply_delay_ticks += ticks_until_last_reply;
  s_last_reply_time = CoreTiming::GetTicks() + result.reply_delay_ticks;

  if (result.send_reply)
    EnqueueReply(address, static_cast<int>(result.reply_delay_ticks));
}

// Happens AS SOON AS IPC gets a new pointer!
void EnqueueRequest(u32 address)
{
  CoreTiming::ScheduleEvent(1000, s_event_enqueue, address | ENQUEUE_REQUEST_FLAG);
}

// Called to send a reply to an IOS syscall
void EnqueueReply(u32 address, int cycles_in_future, CoreTiming::FromThread from)
{
  // IOS writes back the command that was responded to in the FD field.
  Memory::Write_U32(Memory::Read_U32(address), address + 8);
  // IOS also overwrites the command type with the reply type.
  Memory::Write_U32(IPC_REPLY, address);
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
