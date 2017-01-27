// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <istream>
#include <sstream>
#include <tuple>
#include <utility>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/OH0/OH0.h"
#include "Core/IOS/USB/USBV0.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
OH0::OH0(u32 device_id, const std::string& device_name) : USBHost(device_id, device_name)
{
}

OH0::~OH0()
{
  StopThreads();
}

ReturnCode OH0::Open(const OpenRequest& request)
{
  // TODO: get this from WII_IPC_HLE_Interface once we have a proper version system
  const u16 ios_major_version = Memory::Read_U16(0x3140);
  if (ios_major_version == 57 || ios_major_version == 58 || ios_major_version == 59)
    return IPC_EACCES;
  return USBHost::Open(request);
}

IPCCommandResult OH0::IOCtlV(const IOCtlVRequest& request)
{
  INFO_LOG(IOS_USB, "/dev/usb/oh0 - IOCtlV %u", request.request);
  switch (request.request)
  {
  case USB::IOCTLV_USBV0_GETDEVLIST:
  {
    if (!request.HasNumberOfVectors(2, 2))
      return GetDefaultReply(IPC_EINVAL);

    const u8 max_num_descriptors = Memory::Read_U8(request.in_vectors[0].address);
    if (request.io_vectors[1].size != max_num_descriptors * 8)
      return GetDefaultReply(IPC_EINVAL);

    const u8 interface_class = Memory::Read_U8(request.in_vectors[1].address);
    std::vector<u32> buffer(request.io_vectors[1].size);
    const u8 num_devices = GetDeviceList(max_num_descriptors, interface_class, &buffer);
    Memory::Write_U8(num_devices, request.io_vectors[0].address);
    Memory::CopyToEmu(request.io_vectors[1].address, buffer.data(), buffer.size());
    INFO_LOG(IOS_USB, "IOCTLV_USBV0_GETDEVLIST: ret = %d", num_devices);
    return GetDefaultReply(IPC_SUCCESS);
  }

  case USB::IOCTLV_USBV0_GETRHPORTSTATUS:
    if (!request.HasNumberOfVectors(1, 1))
      return GetDefaultReply(IPC_EINVAL);

    ERROR_LOG(IOS_USB, "Unimplemented IOCtlV: IOCTLV_USBV0_GETRHPORTSTATUS");
    request.Dump(GetDeviceName(), LogTypes::IOS_USB, LogTypes::LERROR);
    return GetDefaultReply(IPC_SUCCESS);

  case USB::IOCTLV_USBV0_SETRHPORTSTATUS:
    if (!request.HasNumberOfVectors(2, 0))
      return GetDefaultReply(IPC_EINVAL);

    ERROR_LOG(IOS_USB, "Unimplemented IOCtlV: IOCTLV_USBV0_SETRHPORTSTATUS");
    request.Dump(GetDeviceName(), LogTypes::IOS_USB, LogTypes::LERROR);
    return GetDefaultReply(IPC_SUCCESS);

  case USB::IOCTLV_USBV0_DEVINSERTHOOK:
  {
    if (!request.HasNumberOfVectors(2, 0))
      return GetDefaultReply(IPC_EINVAL);

    std::lock_guard<std::mutex> lock{m_hooks_mutex};
    const u16 vid = Memory::Read_U16(request.in_vectors[0].address);
    const u16 pid = Memory::Read_U16(request.in_vectors[1].address);
    if (HasDeviceWithVidPid(vid, pid))
      return GetDefaultReply(IPC_SUCCESS);
    // TODO: figure out whether IOS allows more than one hook. And proper return code if not.
    if (m_insertion_hooks.count({vid, pid}) != 0)
      return GetDefaultReply(IPC_EEXIST);
    m_insertion_hooks[{vid, pid}] = request.address;
    return GetNoReply();
  }

  case USB::IOCTLV_USBV0_DEVICECLASSCHANGE:
    if (!request.HasNumberOfVectors(1, 0))
      return GetDefaultReply(IPC_EINVAL);
    WARN_LOG(IOS_USB, "Unimplemented IOCtlV: USB::IOCTLV_USBV0_DEVICECLASSCHANGE (no reply)");
    request.Dump(GetDeviceName(), LogTypes::IOS_USB, LogTypes::LWARNING);
    return GetNoReply();

  case USB::IOCTLV_USBV0_DEVINSERTHOOKID:
  {
    if (!request.HasNumberOfVectors(3, 1))
      return GetDefaultReply(IPC_EINVAL);

    std::lock_guard<std::mutex> lock{m_hooks_mutex};
    const u16 vid = Memory::Read_U16(request.in_vectors[0].address);
    const u16 pid = Memory::Read_U16(request.in_vectors[1].address);
    const bool trigger_only_for_new_device = Memory::Read_U8(request.in_vectors[2].address) == 1;
    if (!trigger_only_for_new_device && HasDeviceWithVidPid(vid, pid))
      return GetDefaultReply(IPC_SUCCESS);
    // TODO: figure out whether IOS allows more than one hook (as with ioctlv 27).
    if (m_insertion_hooks.count({vid, pid}) != 0)
      return GetDefaultReply(IPC_EEXIST);
    m_insertion_hooks.insert({{vid, pid}, request.address});
    // The output vector is overwritten with an ID to use with ioctl 31 for cancelling the hook.
    Memory::Write_U32(vid << 16 | pid, request.io_vectors[0].address);
    return GetNoReply();
  }

  default:
    return GetDefaultReply(IPC_EINVAL);
  }
}

IPCCommandResult OH0::IOCtl(const IOCtlRequest& request)
{
  request.Log(GetDeviceName(), LogTypes::IOS_USB);
  switch (request.request)
  {
  case USB::IOCTL_USBV0_GETRHDESCA:
    if (!request.buffer_out || request.buffer_out_size != 4)
      return GetDefaultReply(IPC_EINVAL);

    // Based on a hardware test, this ioctl seems to return a constant value
    Memory::Write_U32(0x02000302, request.buffer_out);
    request.Dump(GetDeviceName(), LogTypes::IOS_USB, LogTypes::LWARNING);
    return GetDefaultReply(IPC_SUCCESS);

  case USB::IOCTL_USBV0_CANCEL_INSERT_HOOK:
  {
    if (!request.buffer_in || request.buffer_in_size != 4)
      return GetDefaultReply(IPC_EINVAL);

    // IOS assigns random IDs, but ours are simply the VID + PID (see
    // USB::IOCTLV_USBV0_DEVINSERTHOOKID)
    TriggerHook(m_insertion_hooks,
                {Memory::Read_U16(request.buffer_in), Memory::Read_U16(request.buffer_in + 2)},
                USB_ECANCELED);
    return GetDefaultReply(IPC_SUCCESS);
  }

  default:
    return GetDefaultReply(IPC_EINVAL);
  }
}

static s32 SubmitTransfer(std::shared_ptr<USB::Device> device, const IOCtlVRequest& ioctlv)
{
  switch (ioctlv.request)
  {
  case USB::IOCTLV_USBV0_CTRLMSG:
    if (!ioctlv.HasNumberOfVectors(6, 1) ||
        Common::swap16(Memory::Read_U16(ioctlv.in_vectors[4].address)) != ioctlv.io_vectors[0].size)
      return IPC_EINVAL;
    return device->SubmitTransfer(std::make_unique<USB::V0CtrlMessage>(ioctlv));

  case USB::IOCTLV_USBV0_BLKMSG:
  case USB::IOCTLV_USBV0_LBLKMSG:
    if (!ioctlv.HasNumberOfVectors(2, 1) ||
        Memory::Read_U16(ioctlv.in_vectors[1].address) != ioctlv.io_vectors[0].size)
      return IPC_EINVAL;
    return device->SubmitTransfer(
        std::make_unique<USB::V0BulkMessage>(ioctlv, ioctlv.request == USB::IOCTLV_USBV0_LBLKMSG));

  case USB::IOCTLV_USBV0_INTRMSG:
    if (!ioctlv.HasNumberOfVectors(2, 1) ||
        Memory::Read_U16(ioctlv.in_vectors[1].address) != ioctlv.io_vectors[0].size)
      return IPC_EINVAL;
    return device->SubmitTransfer(std::make_unique<USB::V0IntrMessage>(ioctlv));

  case USB::IOCTLV_USBV0_ISOMSG:
    if (!ioctlv.HasNumberOfVectors(3, 2))
      return IPC_EINVAL;
    return device->SubmitTransfer(std::make_unique<USB::V0IsoMessage>(ioctlv));

  default:
    return -1;
  }
}

IPCCommandResult OH0::DeviceIOCtlV(const u64 device_id, const IOCtlVRequest& request)
{
  const auto device = GetDeviceById(device_id);
  if (!device)
    return GetDefaultReply(IPC_ENOENT);

  switch (request.request)
  {
  case USB::IOCTLV_USBV0_CTRLMSG:
  case USB::IOCTLV_USBV0_BLKMSG:
  case USB::IOCTLV_USBV0_LBLKMSG:
  case USB::IOCTLV_USBV0_INTRMSG:
  case USB::IOCTLV_USBV0_ISOMSG:
  {
    const s32 ret = SubmitTransfer(device, request);
    if (ret < 0)
    {
      ERROR_LOG(IOS_USB, "[%04x:%04x] Failed to submit transfer (IOCtlV %u): %s", device->GetVid(),
                device->GetPid(), request.request, device->GetErrorName(ret).c_str());
    }
    return GetNoReply();
  }

  case USB::IOCTLV_USBV0_UNKNOWN_32:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_USB);
    return GetDefaultReply(IPC_SUCCESS);

  default:
    return GetDefaultReply(IPC_EINVAL);
  }
}

IPCCommandResult OH0::DeviceIOCtl(const u64 device_id, const IOCtlRequest& request)
{
  const auto device = GetDeviceById(device_id);
  if (!device)
    return GetDefaultReply(IPC_ENOENT);

  switch (request.request)
  {
  case USB::IOCTL_USBV0_DEVREMOVALHOOK:
  {
    std::lock_guard<std::mutex> lock{m_hooks_mutex};
    // IOS only allows a single device removal hook.
    if (m_removal_hooks.count(device_id) != 0)
      return GetDefaultReply(IPC_EEXIST);
    m_removal_hooks.insert({device_id, request.address});
    return GetNoReply();
  }

  // Unimplemented because libusb doesn't do power management.
  case USB::IOCTL_USBV0_SUSPENDDEV:
  case USB::IOCTL_USBV0_RESUMEDEV:
    return GetDefaultReply(IPC_SUCCESS);

  case USB::IOCTL_USBV0_RESET_DEVICE:
    TriggerHook(m_removal_hooks, device_id, IPC_SUCCESS);
    return GetDefaultReply(IPC_SUCCESS);

  default:
    return GetDefaultReply(IPC_EINVAL);
  }
}

void OH0::DoState(PointerWrap& p)
{
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    Core::DisplayMessage("It is suggested that you unplug and replug all connected USB devices.",
                         5000);
    Core::DisplayMessage("If USB doesn't work properly, an emulation reset may be needed.", 5000);
  }
  p.Do(m_insertion_hooks);
  p.Do(m_removal_hooks);
  USBHost::DoState(p);
}

bool OH0::HasDeviceWithVidPid(const u16 vid, const u16 pid) const
{
  return std::find_if(m_devices.begin(), m_devices.end(), [&](const auto& device) {
           return device.second->GetVid() == vid && device.second->GetPid() == pid;
         }) != m_devices.end();
}

u8 OH0::GetDeviceList(const u8 max_num, const u8 interface_class, std::vector<u32>* buffer) const
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  u8 num_devices = 0;
  for (const auto& device : m_devices)
  {
    if (num_devices >= max_num)
    {
      WARN_LOG(IOS_USB, "Too many devices connected (%d ≥ %d), skipping", num_devices, max_num);
      break;
    }

    if (!device.second->HasClass(interface_class))
      continue;

    const u16 vid = Common::swap16(device.second->GetVid());
    const u32 pid = Common::swap16(device.second->GetPid()) << 16;
    std::memcpy(buffer->data() + num_devices * 2 + 1, &pid, sizeof(pid));
    std::memcpy(buffer->data() + num_devices * 2 + 1, &vid, sizeof(vid));

    ++num_devices;
  }
  return num_devices;
}

template <typename T>
void OH0::TriggerHook(std::map<T, u32>& hooks, T value, const ReturnCode return_value)
{
  std::lock_guard<std::mutex> lk{m_hooks_mutex};
  const auto hook = hooks.find(value);
  if (hook == hooks.end())
    return;
  EnqueueReply(Request{hook->second}, return_value, 0, CoreTiming::FromThread::ANY);
  hooks.erase(hook);
}

void OH0::OnDeviceChange(const ChangeEvent event, std::shared_ptr<USB::Device> device)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  if (event == ChangeEvent::Inserted)
    TriggerHook(m_insertion_hooks, {device->GetVid(), device->GetPid()}, IPC_SUCCESS);
  else if (event == ChangeEvent::Removed)
    TriggerHook(m_removal_hooks, device->GetId(), IPC_SUCCESS);
}

static std::pair<u16, u16> GetVidPidFromDevicePath(const std::string& device_path)
{
  u16 vid, pid;
  std::stringstream stream{device_path};
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

std::pair<ReturnCode, u64> OH0::DeviceOpen(const u16 vid, const u16 pid)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);

  bool has_device_with_vid_pid = false;
  for (const auto& device : m_devices)
  {
    if (device.second->GetVid() != vid || device.second->GetPid() != pid)
      continue;
    has_device_with_vid_pid = true;

    if (m_opened_devices.count(device.second->GetId()) != 0 || !device.second->Attach())
      continue;

    m_opened_devices.emplace(device.second->GetId());
    return {IPC_SUCCESS, device.second->GetId()};
  }
  // IOS doesn't allow opening the same device more than once (IPC_EEXIST)
  return {has_device_with_vid_pid ? IPC_EEXIST : IPC_ENOENT, 0};
}

void OH0::DeviceClose(const u64 device_id)
{
  TriggerHook(m_removal_hooks, device_id, IPC_ENOENT);
  m_opened_devices.erase(device_id);
}

OH0Device::OH0Device(u32 device_id, const std::string& device_name)
    : Device(device_id, device_name, DeviceType::OH0)
{
  if (device_name.empty())
    return;
  std::tie(m_vid, m_pid) = GetVidPidFromDevicePath(device_name);
}

void OH0Device::DoState(PointerWrap& p)
{
  const auto oh0 = GetDeviceByName("/dev/usb/oh0");
  m_oh0 = std::static_pointer_cast<OH0>(oh0);
  p.Do(m_name);
  p.Do(m_vid);
  p.Do(m_pid);
  p.Do(m_device_id);
}

ReturnCode OH0Device::Open(const OpenRequest& request)
{
  const auto oh0 = GetDeviceByName("/dev/usb/oh0");
  m_oh0 = std::static_pointer_cast<OH0>(oh0);

  ReturnCode return_code;
  std::tie(return_code, m_device_id) = m_oh0->DeviceOpen(m_vid, m_pid);
  return return_code;
}

void OH0Device::Close()
{
  m_oh0->DeviceClose(m_device_id);
}

IPCCommandResult OH0Device::IOCtl(const IOCtlRequest& request)
{
  request.Log(GetDeviceName(), LogTypes::IOS_USB);
  return m_oh0->DeviceIOCtl(m_device_id, request);
}

IPCCommandResult OH0Device::IOCtlV(const IOCtlVRequest& request)
{
  return m_oh0->DeviceIOCtlV(m_device_id, request);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
