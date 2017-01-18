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
#include "Core/IOS/DI/DI.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/DeviceStub.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/FS/FS.h"
#include "Core/IOS/FS/FileIO.h"
#include "Core/IOS/IPC.h"
#include "Core/IOS/Network/Net.h"
#include "Core/IOS/Network/SSL.h"
#include "Core/IOS/SDIO/SDIOSlot0.h"
#include "Core/IOS/STM/STM.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/BTReal.h"
#include "Core/IOS/USB/USB_KBD.h"
#include "Core/IOS/USB/USB_VEN.h"
#include "Core/IOS/WFS/WFSI.h"
#include "Core/IOS/WFS/WFSSRV.h"

namespace CoreTiming
{
struct EventType;
}  // namespace CoreTiming

#if defined(__LIBUSB__)
#include "Core/IOS/USB/USB_HIDv4.h"
#endif

namespace IOS
{
namespace HLE
{
static std::map<u32, std::shared_ptr<Device::Device>> s_device_map;
static std::mutex s_device_map_mutex;

// STATE_TO_SAVE
constexpr u8 IPC_MAX_FDS = 0x18;
constexpr u8 ES_MAX_COUNT = 3;
static std::shared_ptr<Device::Device> s_fdmap[IPC_MAX_FDS];
static std::shared_ptr<Device::ES> s_es_handles[ES_MAX_COUNT];

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
  auto device = static_cast<Device::SDIOSlot0*>(GetDeviceByName("/dev/sdio/slot0").get());
  if (device)
    device->EventNotify();
}

static u32 num_devices;

template <typename T>
std::shared_ptr<T> AddDevice(const char* device_name)
{
  auto device = std::make_shared<T>(num_devices, device_name);
  _assert_(device->GetDeviceType() == Device::Device::DeviceType::Static);
  s_device_map[num_devices] = device;
  num_devices++;
  return device;
}

void Reinit()
{
  std::lock_guard<std::mutex> lock(s_device_map_mutex);
  _assert_msg_(WII_IPC_HLE, s_device_map.empty(), "Reinit called while already initialized");
  Device::ES::m_ContentFile = "";

  num_devices = 0;

  // Build hardware devices
  if (!SConfig::GetInstance().m_bt_passthrough_enabled)
    AddDevice<Device::BluetoothEmu>("/dev/usb/oh1/57e/305");
  else
    AddDevice<Device::BluetoothReal>("/dev/usb/oh1/57e/305");

  AddDevice<Device::STMImmediate>("/dev/stm/immediate");
  AddDevice<Device::STMEventHook>("/dev/stm/eventhook");
  AddDevice<Device::FS>("/dev/fs");

  // IOS allows two ES devices at a time
  for (auto& es_device : s_es_handles)
    es_device = AddDevice<Device::ES>("/dev/es");

  AddDevice<Device::DI>("/dev/di");
  AddDevice<Device::NetKDRequest>("/dev/net/kd/request");
  AddDevice<Device::NetKDTime>("/dev/net/kd/time");
  AddDevice<Device::NetNCDManage>("/dev/net/ncd/manage");
  AddDevice<Device::NetWDCommand>("/dev/net/wd/command");
  AddDevice<Device::NetIPTop>("/dev/net/ip/top");
  AddDevice<Device::NetSSL>("/dev/net/ssl");
  AddDevice<Device::USB_KBD>("/dev/usb/kbd");
  AddDevice<Device::USB_VEN>("/dev/usb/ven");
  AddDevice<Device::SDIOSlot0>("/dev/sdio/slot0");
  AddDevice<Device::Stub>("/dev/sdio/slot1");
#if defined(__LIBUSB__)
  AddDevice<Device::USB_HIDv4>("/dev/usb/hid");
#else
  AddDevice<Device::Stub>("/dev/usb/hid");
#endif
  AddDevice<Device::Stub>("/dev/usb/oh1");
  AddDevice<Device::WFSSRV>("/dev/usb/wfssrv");
  AddDevice<Device::WFSI>("/dev/wfsi");
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
    device->Close();
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
  Device::ES::ES_DIVerify(tmd);
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

std::shared_ptr<Device::Device> GetDeviceByName(const std::string& device_name)
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

std::shared_ptr<Device::Device> AccessDeviceByID(u32 id)
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

  // We need to make sure all file handles are closed so IOS::HLE::Device::FS::DoState can
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
        auto device_type = Device::Device::DeviceType::Static;
        p.Do(device_type);
        switch (device_type)
        {
        case Device::Device::DeviceType::Static:
        {
          u32 device_id = 0;
          p.Do(device_id);
          s_fdmap[i] = AccessDeviceByID(device_id);
          break;
        }
        case Device::Device::DeviceType::FileIO:
          s_fdmap[i] = std::make_shared<Device::FileIO>(i, "");
          s_fdmap[i]->DoState(p);
          break;
        }
      }
    }

    for (auto& es_device : s_es_handles)
    {
      const u32 handle_id = es_device->GetDeviceID();
      p.Do(handle_id);
      es_device = std::static_pointer_cast<Device::ES>(AccessDeviceByID(handle_id));
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
        if (device_type == Device::Device::DeviceType::Static)
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

static std::shared_ptr<Device::Device> GetUnusedESDevice()
{
  const auto iterator = std::find_if(std::begin(s_es_handles), std::end(s_es_handles),
                                     [](const auto& es_device) { return !es_device->IsOpened(); });
  return (iterator != std::end(s_es_handles)) ? *iterator : nullptr;
}

// Returns the FD for the newly opened device (on success) or an error code.
static s32 OpenDevice(const IOSOpenRequest& request)
{
  const s32 new_fd = GetFreeDeviceID();
  INFO_LOG(WII_IPC_HLE, "Opening %s (mode %d, fd %d)", request.path.c_str(), request.flags, new_fd);
  if (new_fd < 0 || new_fd >= IPC_MAX_FDS)
  {
    ERROR_LOG(WII_IPC_HLE, "Couldn't get a free fd, too many open files");
    return FS_EFDEXHAUSTED;
  }

  std::shared_ptr<Device::Device> device;
  if (request.path == "/dev/es")
  {
    device = GetUnusedESDevice();
    if (!device)
      return IPC_EESEXHAUSTED;
  }
  else if (request.path.find("/dev/") == 0)
  {
    device = GetDeviceByName(request.path);
  }
  else if (request.path.find('/') == 0)
  {
    device = std::make_shared<Device::FileIO>(new_fd, request.path);
  }

  if (!device)
  {
    ERROR_LOG(WII_IPC_HLE, "Unknown device: %s", request.path.c_str());
    return IPC_ENOENT;
  }

  const IOSReturnCode code = device->Open(request);
  if (code < IPC_SUCCESS)
    return code;
  s_fdmap[new_fd] = device;
  return new_fd;
}

static IPCCommandResult HandleCommand(const IOSRequest& request)
{
  if (request.command == IPC_CMD_OPEN)
  {
    IOSOpenRequest open_request{request.address};
    const s32 new_fd = OpenDevice(open_request);
    return Device::Device::GetDefaultReply(new_fd);
  }

  const auto device = (request.fd < IPC_MAX_FDS) ? s_fdmap[request.fd] : nullptr;
  if (!device)
    return Device::Device::GetDefaultReply(IPC_EINVAL);

  switch (request.command)
  {
  case IPC_CMD_CLOSE:
    s_fdmap[request.fd].reset();
    device->Close();
    return Device::Device::GetDefaultReply(IPC_SUCCESS);
  case IPC_CMD_READ:
    return device->Read(IOSReadWriteRequest{request.address});
  case IPC_CMD_WRITE:
    return device->Write(IOSReadWriteRequest{request.address});
  case IPC_CMD_SEEK:
    return device->Seek(IOSSeekRequest{request.address});
  case IPC_CMD_IOCTL:
    return device->IOCtl(IOSIOCtlRequest{request.address});
  case IPC_CMD_IOCTLV:
    return device->IOCtlV(IOSIOCtlVRequest{request.address});
  default:
    _assert_msg_(WII_IPC_HLE, false, "Unexpected command: %x", request.command);
    return Device::Device::GetDefaultReply(IPC_EINVAL);
  }
}

void ExecuteCommand(const u32 address)
{
  IOSRequest request{address};
  IPCCommandResult result = HandleCommand(request);

  // Ensure replies happen in order
  const s64 ticks_until_last_reply = s_last_reply_time - CoreTiming::GetTicks();
  if (ticks_until_last_reply > 0)
    result.reply_delay_ticks += ticks_until_last_reply;
  s_last_reply_time = CoreTiming::GetTicks() + result.reply_delay_ticks;

  if (result.send_reply)
    EnqueueReply(request, result.return_value, static_cast<int>(result.reply_delay_ticks));
}

// Happens AS SOON AS IPC gets a new pointer!
void EnqueueRequest(u32 address)
{
  CoreTiming::ScheduleEvent(1000, s_event_enqueue, address | ENQUEUE_REQUEST_FLAG);
}

// Called to send a reply to an IOS syscall
void EnqueueReply(const IOSRequest& request, const s32 return_value, int cycles_in_future,
                  CoreTiming::FromThread from)
{
  Memory::Write_U32(static_cast<u32>(return_value), request.address + 4);
  // IOS writes back the command that was responded to in the FD field.
  Memory::Write_U32(request.command, request.address + 8);
  // IOS also overwrites the command type with the reply type.
  Memory::Write_U32(IPC_REPLY, request.address);
  CoreTiming::ScheduleEvent(cycles_in_future, s_event_enqueue, request.address, from);
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
  if (!IsReady())
    return;

  if (s_request_queue.size())
  {
    GenerateAck(s_request_queue.front());
    DEBUG_LOG(WII_IPC_HLE, "||-- Acknowledge IPC Request @ 0x%08x", s_request_queue.front());
    u32 command = s_request_queue.front();
    s_request_queue.pop_front();
    ExecuteCommand(command);
    return;
  }

  if (s_reply_queue.size())
  {
    GenerateReply(s_reply_queue.front());
    DEBUG_LOG(WII_IPC_HLE, "<<-- Reply to IPC Request @ 0x%08x", s_reply_queue.front());
    s_reply_queue.pop_front();
    return;
  }

  if (s_ack_queue.size())
  {
    GenerateAck(s_ack_queue.front());
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
}  // namespace HLE
}  // namespace IOS
