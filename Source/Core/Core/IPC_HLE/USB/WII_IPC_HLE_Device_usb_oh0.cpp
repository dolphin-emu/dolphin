// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <istream>
#include <memory>
#include <tuple>
#include <utility>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/USB/Common.h"
#include "Core/IPC_HLE/USB/USBV0.h"
#include "Core/IPC_HLE/USB/WII_IPC_HLE_Device_usb_oh0.h"

CWII_IPC_HLE_Device_usb_oh0::CWII_IPC_HLE_Device_usb_oh0(u32 device_id,
                                                         const std::string& device_name)
    : CWII_IPC_HLE_Device_usb_host(device_id, device_name)
{
  m_ready_to_trigger_hooks.Set();
}

CWII_IPC_HLE_Device_usb_oh0::~CWII_IPC_HLE_Device_usb_oh0()
{
  StopThreads();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0::IOCtlV(const u32 command_address)
{
  const SIOCtlVBuffer cmd_buffer(command_address);
  Memory::Write_U32(FS_SUCCESS, command_address + 4);
  INFO_LOG(WII_IPC_USB, "/dev/usb/oh0 - IOCtlV %d", cmd_buffer.Parameter);
  switch (cmd_buffer.Parameter)
  {
  case USBV0_IOCTL_GETDEVLIST:
  {
    if (Core::g_want_determinism)
    {
      // Indicate that there are no devices connected.
      Memory::Write_U8(0, cmd_buffer.PayloadBuffer[0].m_Address);
      return GetDefaultReply();
    }

    const u8 max_num_descriptors = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
    const u8 interface_class = Memory::Read_U8(cmd_buffer.InBuffer[1].m_Address);
    std::vector<u32> buffer(cmd_buffer.PayloadBuffer[1].m_Size);
    const u8 num_devices = GetDeviceList(max_num_descriptors, interface_class, &buffer);
    Memory::Write_U8(num_devices, cmd_buffer.PayloadBuffer[0].m_Address);
    Memory::CopyToEmu(cmd_buffer.PayloadBuffer[1].m_Address, buffer.data(), buffer.size());
    INFO_LOG(WII_IPC_USB, "GETDEVLIST: %d device%s", num_devices, num_devices == 1 ? "" : "s");
    return GetDefaultReply();
  }

  case USBV0_IOCTL_SUSPENDDEV:
  case USBV0_IOCTL_RESUMEDEV:
  case USBV0_IOCTL_DEVICECLASSCHANGE:
    WARN_LOG(WII_IPC_USB, "Unimplemented IOCtlV: %d", cmd_buffer.Parameter);
    return GetDefaultReply();

  case USBV0_IOCTL_DEVINSERTHOOK:
  // XXX: figure out if this ioctlv does something else
  // Since it is called right after opening /dev/usb/oh0 with a VID/PID, the current guess
  // is that this may be similar to USBV0_IOCTL_DEVINSERTHOOK.
  // And in practice, treating it the same as USBV0_IOCTL_DEVINSERTHOOK seems to work.
  case USBV0_IOCTL_UNKNOWN_30:
  {
    // Ignore any hook (and never trigger them since no device should be connected).
    if (Core::g_want_determinism)
      return GetNoReply();

    std::lock_guard<std::mutex> lock{m_hooks_mutex};
    const u16 vid = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address);
    const u16 pid = Memory::Read_U16(cmd_buffer.InBuffer[1].m_Address);
    if (DoesDeviceExist(vid, pid))
      return GetDefaultReply();
    m_insertion_hooks[{vid, pid}] = command_address;
    return GetNoReply();
  }

  default:
    ERROR_LOG(WII_IPC_USB, "Unknown IOCtlV: %d", cmd_buffer.Parameter);
    DumpAsync(cmd_buffer.BufferVector, cmd_buffer.NumberInBuffer, cmd_buffer.NumberPayloadBuffer,
              LogTypes::WII_IPC_USB, LogTypes::LERROR);
    return GetDefaultReply();
  }
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0::IOCtl(const u32 command_address)
{
  Memory::Write_U32(FS_EINVAL, command_address + 4);
  return GetDefaultReply();
}

static int SubmitTransfer(std::shared_ptr<USBDevice> device, const SIOCtlVBuffer& cmd_buffer)
{
  switch (cmd_buffer.Parameter)
  {
  case USBV0_IOCTL_CTRLMSG:
    return device->SubmitTransfer(std::make_unique<USBV0CtrlMessage>(cmd_buffer));
  case USBV0_IOCTL_BLKMSG:
    return device->SubmitTransfer(std::make_unique<USBV0BulkMessage>(cmd_buffer));
  case USBV0_IOCTL_INTRMSG:
    return device->SubmitTransfer(std::make_unique<USBV0IntrMessage>(cmd_buffer));
  case USBV0_IOCTL_ISOMSG:
    return device->SubmitTransfer(std::make_unique<USBV0IsoMessage>(cmd_buffer));
  default:
    return -1;
  }
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0::DeviceIOCtlV(const u16 vid, const u16 pid,
                                                           const u32 command_address)
{
  const auto device = GetDeviceByVidPid(vid, pid);
  if (!device)
  {
    Memory::Write_U32(FS_ENOENT, command_address + 4);
    return GetDefaultReply();
  }

  const SIOCtlVBuffer cmd_buffer(command_address);
  Memory::Write_U32(FS_SUCCESS, command_address + 4);
  switch (cmd_buffer.Parameter)
  {
  case USBV0_IOCTL_CTRLMSG:
  case USBV0_IOCTL_BLKMSG:
  case USBV0_IOCTL_INTRMSG:
  case USBV0_IOCTL_ISOMSG:
  {
    const int ret = SubmitTransfer(device, cmd_buffer);
    if (ret < 0)
    {
      ERROR_LOG(WII_IPC_USB, "[%04x:%04x] Failed to submit transfer (IOCtlV %d): %s",
                device->GetVid(), device->GetPid(), cmd_buffer.Parameter,
                device->GetErrorName(ret).c_str());
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    Memory::Write_U32(FS_SUCCESS, command_address + 4);
    return GetNoReply();
  }
  default:
    ERROR_LOG(WII_IPC_USB, "[%04x:%04x] Unknown IOCtlV: %02x", device->GetVid(), device->GetPid(),
              cmd_buffer.Parameter);
    DumpAsync(cmd_buffer.BufferVector, cmd_buffer.NumberInBuffer, cmd_buffer.NumberPayloadBuffer,
              LogTypes::WII_IPC_USB, LogTypes::LERROR);
    return GetDefaultReply();
  }
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0::DeviceIOCtl(const u16 vid, const u16 pid,
                                                          const u32 command_address)
{
  const IOCtlBuffer cmd_buffer(command_address);
  Memory::Write_U32(FS_SUCCESS, command_address + 4);
  INFO_LOG(WII_IPC_USB, "/dev/usb/oh0 (sub-device) - IOCtl %d", cmd_buffer.m_request);
  switch (cmd_buffer.m_request)
  {
  case USBV0_IOCTL_DEVREMOVALHOOK:
  {
    std::lock_guard<std::mutex> lock{m_hooks_mutex};
    m_removal_hooks.insert({{vid, pid}, command_address});
    return GetNoReply();
  }

  case USBV0_IOCTL_UNKNOWN_29:
    // This is used by Wheel of Fortune on shutdown.
    // No parameters. Perhaps this is used as a shutdown ioctl.
    return GetDefaultReply();

  default:
    ERROR_LOG(WII_IPC_USB, "Unknown IOCtl (%04x:%04x): %d", vid, pid, cmd_buffer.m_request);
    ERROR_LOG(WII_IPC_USB, "In\n%s", ArrayToString(Memory::GetPointer(cmd_buffer.m_in_buffer_addr),
                                                   cmd_buffer.m_in_buffer_size)
                                         .c_str());
    ERROR_LOG(WII_IPC_USB, "Out\n%s",
              ArrayToString(Memory::GetPointer(cmd_buffer.m_out_buffer_addr),
                            cmd_buffer.m_out_buffer_size)
                  .c_str());
    return GetDefaultReply();
  }
}

void CWII_IPC_HLE_Device_usb_oh0::DoState(PointerWrap& p)
{
  CWII_IPC_HLE_Device_usb_host::DoState(p);
  p.Do(m_insertion_hooks);
  p.Do(m_removal_hooks);
}

bool CWII_IPC_HLE_Device_usb_oh0::AttachDevice(const u16 vid, const u16 pid) const
{
  const auto device = GetDeviceByVidPid(vid, pid);
  if (!device)
    return false;
  return device->AttachDevice();
}

bool CWII_IPC_HLE_Device_usb_oh0::DoesDeviceExist(const u16 vid, const u16 pid) const
{
  return GetDeviceByVidPid(vid, pid) != nullptr;
}

u8 CWII_IPC_HLE_Device_usb_oh0::GetDeviceList(const u8 max_num, const u8 interface_class,
                                              std::vector<u32>* buffer) const
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  u8 num_devices = 0;
  for (const auto& device : m_devices)
  {
    if (num_devices >= max_num)
    {
      WARN_LOG(WII_IPC_USB, "Too many devices connected (%d ≥ %d), skipping", num_devices, max_num);
      break;
    }

    if (device.second->GetInterfaceClass() != interface_class)
      continue;

    const u16 vid = Common::swap16(device.second->GetVid());
    const u32 pid = Common::swap16(device.second->GetPid()) << 16;
    std::memcpy(buffer->data() + num_devices * 2 + 1, &pid, sizeof(pid));
    std::memcpy(buffer->data() + num_devices * 2 + 1, &vid, sizeof(vid));

    ++num_devices;
  }
  return num_devices;
}

static void TriggerHook(std::map<USBDeviceInfo, u32>& hooks, u16 vid, u16 pid)
{
  const auto hook = hooks.find({vid, pid});
  if (hook == hooks.end())
    return;

  INFO_LOG(WII_IPC_USB, "Triggered hook for %04x:%04x", vid, pid);
  Memory::Write_U32(FS_SUCCESS, hook->second + 4);
  WII_IPC_HLE_Interface::EnqueueReply(hook->second, 0, CoreTiming::FromThread::NON_CPU);
  hooks.erase(hook);
}

void CWII_IPC_HLE_Device_usb_oh0::OnDeviceChange(const ChangeEvent event,
                                                 std::shared_ptr<USBDevice> device)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  if (event == ChangeEvent::Inserted)
    TriggerHook(m_insertion_hooks, device->GetVid(), device->GetPid());
  else if (event == ChangeEvent::Removed)
    TriggerHook(m_removal_hooks, device->GetVid(), device->GetPid());
}

static std::pair<u16, u16> GetVidPidFromDevicePath(const std::string& device_path)
{
  u16 vid, pid;
  std::stringstream stream(device_path);
  std::string segment;
  std::vector<std::string> list;
  while (std::getline(stream, segment, '/'))
    if (!segment.empty())
      list.push_back(segment);

  std::stringstream ss;
  ss << std::hex << list[3];
  ss >> vid;
  ss.clear();
  ss << std::hex << list[4];
  ss >> pid;
  return {vid, pid};
}

CWII_IPC_HLE_Device_usb_oh0_device::CWII_IPC_HLE_Device_usb_oh0_device(
    u32 device_id, const std::string& device_name)
    : IWII_IPC_HLE_Device(device_id, device_name)
{
  std::tie(m_vid, m_pid) = GetVidPidFromDevicePath(device_name);
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_device::Open(const u32 command_address, const u32 mode)
{
  if (m_Active)
  {
    Memory::Write_U32(FS_EACCES, command_address + 4);
    return GetDefaultReply();
  }

  const auto oh0 = WII_IPC_HLE_Interface::GetDeviceByName("/dev/usb/oh0");
  _assert_msg_(WII_IPC_USB, oh0 != nullptr, "/dev/usb/oh0 does not exist");
  m_oh0 = std::static_pointer_cast<CWII_IPC_HLE_Device_usb_oh0>(oh0);

  Memory::Write_U32(FS_ENOENT, command_address + 4);
  if (Core::g_want_determinism || !m_oh0->DoesDeviceExist(m_vid, m_pid) ||
      !m_oh0->AttachDevice(m_vid, m_pid))
    return GetDefaultReply();

  Memory::Write_U32(GetDeviceID(), command_address + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_device::Close(const u32 cmd_address, const bool force)
{
  if (!force)
    Memory::Write_U32(0, cmd_address + 4);
  m_Active = false;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_device::IOCtl(const u32 command_address)
{
  return m_oh0->DeviceIOCtl(m_vid, m_pid, command_address);
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh0_device::IOCtlV(const u32 command_address)
{
  return m_oh0->DeviceIOCtlV(m_vid, m_pid, command_address);
}
