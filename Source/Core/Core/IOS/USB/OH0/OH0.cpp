// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/OH0/OH0.h"

#include <algorithm>
#include <cstring>
#include <istream>
#include <sstream>
#include <tuple>
#include <utility>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/USBV0.h"
#include "Core/IOS/VersionInfo.h"
#include "Core/System.h"

namespace IOS::HLE
{
OH0::OH0(EmulationKernel& ios, const std::string& device_name) : USBHost(ios, device_name)
{
}

OH0::~OH0()
{
  m_scan_thread.Stop();
}

std::optional<IPCReply> OH0::Open(const OpenRequest& request)
{
  if (HasFeature(m_ios.GetVersion(), Feature::NewUSB))
    return IPCReply(IPC_EACCES);
  return USBHost::Open(request);
}

std::optional<IPCReply> OH0::IOCtl(const IOCtlRequest& request)
{
  request.Log(GetDeviceName(), Common::Log::LogType::IOS_USB);
  switch (request.request)
  {
  case USB::IOCTL_USBV0_GETRHDESCA:
    return GetRhDesca(request);
  case USB::IOCTL_USBV0_CANCEL_INSERT_HOOK:
    return CancelInsertionHook(request);
  default:
    return IPCReply(IPC_EINVAL);
  }
}

std::optional<IPCReply> OH0::IOCtlV(const IOCtlVRequest& request)
{
  INFO_LOG_FMT(IOS_USB, "/dev/usb/oh0 - IOCtlV {}", request.request);
  switch (request.request)
  {
  case USB::IOCTLV_USBV0_GETDEVLIST:
    return GetDeviceList(request);
  case USB::IOCTLV_USBV0_GETRHPORTSTATUS:
    return GetRhPortStatus(request);
  case USB::IOCTLV_USBV0_SETRHPORTSTATUS:
    return SetRhPortStatus(request);
  case USB::IOCTLV_USBV0_DEVINSERTHOOK:
    return RegisterInsertionHook(request);
  case USB::IOCTLV_USBV0_DEVICECLASSCHANGE:
    return RegisterClassChangeHook(request);
  case USB::IOCTLV_USBV0_DEVINSERTHOOKID:
    return RegisterInsertionHookWithID(request);
  default:
    return IPCReply(IPC_EINVAL);
  }
}

void OH0::DoState(PointerWrap& p)
{
  if (p.IsReadMode() && !m_devices.empty())
  {
    Core::DisplayMessage("It is suggested that you unplug and replug all connected USB devices.",
                         5000);
    Core::DisplayMessage("If USB doesn't work properly, an emulation reset may be needed.", 5000);
  }
  p.Do(m_insertion_hooks);
  p.Do(m_removal_hooks);
  p.Do(m_opened_devices);
  USBHost::DoState(p);
}

IPCReply OH0::CancelInsertionHook(const IOCtlRequest& request)
{
  if (!request.buffer_in || request.buffer_in_size != 4)
    return IPCReply(IPC_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  // IOS assigns random IDs, but ours are simply the VID + PID (see RegisterInsertionHookWithID)
  TriggerHook(m_insertion_hooks,
              {memory.Read_U16(request.buffer_in), memory.Read_U16(request.buffer_in + 2)},
              USB_ECANCELED);
  return IPCReply(IPC_SUCCESS);
}

IPCReply OH0::GetDeviceList(const IOCtlVRequest& request) const
{
  if (!request.HasNumberOfValidVectors(2, 2))
    return IPCReply(IPC_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u8 max_entries_count = memory.Read_U8(request.in_vectors[0].address);
  if (request.io_vectors[1].size != max_entries_count * sizeof(DeviceEntry))
    return IPCReply(IPC_EINVAL);

  const u8 interface_class = memory.Read_U8(request.in_vectors[1].address);
  u8 entries_count = 0;
  std::lock_guard lk(m_devices_mutex);
  for (const auto& device : m_devices)
  {
    if (entries_count >= max_entries_count)
      break;
    if (!device.second->HasClass(interface_class))
      continue;

    DeviceEntry entry;
    entry.unknown = 0;
    entry.vid = Common::swap16(device.second->GetVid());
    entry.pid = Common::swap16(device.second->GetPid());
    memory.CopyToEmu(request.io_vectors[1].address + 8 * entries_count++, &entry, 8);
  }
  memory.Write_U8(entries_count, request.io_vectors[0].address);
  return IPCReply(IPC_SUCCESS);
}

IPCReply OH0::GetRhDesca(const IOCtlRequest& request) const
{
  if (!request.buffer_out || request.buffer_out_size != 4)
    return IPCReply(IPC_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  // Based on a hardware test, this ioctl seems to return a constant value
  memory.Write_U32(0x02000302, request.buffer_out);
  request.Dump(system, GetDeviceName(), Common::Log::LogType::IOS_USB,
               Common::Log::LogLevel::LWARNING);
  return IPCReply(IPC_SUCCESS);
}

IPCReply OH0::GetRhPortStatus(const IOCtlVRequest& request) const
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return IPCReply(IPC_EINVAL);

  ERROR_LOG_FMT(IOS_USB, "Unimplemented IOCtlV: IOCTLV_USBV0_GETRHPORTSTATUS");
  request.Dump(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_USB,
               Common::Log::LogLevel::LERROR);
  return IPCReply(IPC_SUCCESS);
}

IPCReply OH0::SetRhPortStatus(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0))
    return IPCReply(IPC_EINVAL);

  ERROR_LOG_FMT(IOS_USB, "Unimplemented IOCtlV: IOCTLV_USBV0_SETRHPORTSTATUS");
  request.Dump(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_USB,
               Common::Log::LogLevel::LERROR);
  return IPCReply(IPC_SUCCESS);
}

std::optional<IPCReply> OH0::RegisterRemovalHook(const u64 device_id, const IOCtlRequest& request)
{
  std::lock_guard lock{m_hooks_mutex};
  // IOS only allows a single device removal hook.
  if (m_removal_hooks.find(device_id) != m_removal_hooks.end())
    return IPCReply(IPC_EEXIST);
  m_removal_hooks.insert({device_id, request.address});
  return std::nullopt;
}

std::optional<IPCReply> OH0::RegisterInsertionHook(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0))
    return IPCReply(IPC_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u16 vid = memory.Read_U16(request.in_vectors[0].address);
  const u16 pid = memory.Read_U16(request.in_vectors[1].address);
  if (HasDeviceWithVidPid(vid, pid))
    return IPCReply(IPC_SUCCESS);

  std::lock_guard lock{m_hooks_mutex};
  // TODO: figure out whether IOS allows more than one hook.
  m_insertion_hooks[{vid, pid}] = request.address;
  return std::nullopt;
}

std::optional<IPCReply> OH0::RegisterInsertionHookWithID(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 1))
    return IPCReply(IPC_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  std::lock_guard lock{m_hooks_mutex};
  const u16 vid = memory.Read_U16(request.in_vectors[0].address);
  const u16 pid = memory.Read_U16(request.in_vectors[1].address);
  const bool trigger_only_for_new_device = memory.Read_U8(request.in_vectors[2].address) == 1;
  if (!trigger_only_for_new_device && HasDeviceWithVidPid(vid, pid))
    return IPCReply(IPC_SUCCESS);
  // TODO: figure out whether IOS allows more than one hook.
  m_insertion_hooks.insert({{vid, pid}, request.address});
  // The output vector is overwritten with an ID to use with ioctl 31 for cancelling the hook.
  memory.Write_U32(vid << 16 | pid, request.io_vectors[0].address);
  return std::nullopt;
}

std::optional<IPCReply> OH0::RegisterClassChangeHook(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return IPCReply(IPC_EINVAL);
  WARN_LOG_FMT(IOS_USB, "Unimplemented IOCtlV: USB::IOCTLV_USBV0_DEVICECLASSCHANGE (no reply)");
  request.Dump(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_USB,
               Common::Log::LogLevel::LWARNING);
  return std::nullopt;
}

bool OH0::HasDeviceWithVidPid(const u16 vid, const u16 pid) const
{
  return std::any_of(m_devices.begin(), m_devices.end(), [=](const auto& device) {
    return device.second->GetVid() == vid && device.second->GetPid() == pid;
  });
}

void OH0::OnDeviceChange(const ChangeEvent event, std::shared_ptr<USB::Device> device)
{
  std::lock_guard lk(m_devices_mutex);
  if (event == ChangeEvent::Inserted)
    TriggerHook(m_insertion_hooks, {device->GetVid(), device->GetPid()}, IPC_SUCCESS);
  else if (event == ChangeEvent::Removed)
    TriggerHook(m_removal_hooks, device->GetId(), IPC_SUCCESS);
}

template <typename T>
void OH0::TriggerHook(std::map<T, u32>& hooks, T value, const ReturnCode return_value)
{
  std::lock_guard lk{m_hooks_mutex};
  const auto hook = hooks.find(value);
  if (hook == hooks.end())
    return;
  GetEmulationKernel().EnqueueIPCReply(Request{GetSystem(), hook->second}, return_value, 0,
                                       CoreTiming::FromThread::ANY);
  hooks.erase(hook);
}

std::pair<ReturnCode, u64> OH0::DeviceOpen(const u16 vid, const u16 pid)
{
  std::lock_guard lk(m_devices_mutex);

  bool has_device_with_vid_pid = false;
  for (const auto& device : m_devices)
  {
    if (device.second->GetVid() != vid || device.second->GetPid() != pid)
      continue;
    has_device_with_vid_pid = true;

    if (m_opened_devices.find(device.second->GetId()) != m_opened_devices.cend() ||
        !device.second->Attach())
    {
      continue;
    }

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

std::optional<IPCReply> OH0::DeviceIOCtl(const u64 device_id, const IOCtlRequest& request)
{
  const auto device = GetDeviceById(device_id);
  if (!device)
    return IPCReply(IPC_ENOENT);

  switch (request.request)
  {
  case USB::IOCTL_USBV0_DEVREMOVALHOOK:
    return RegisterRemovalHook(device_id, request);
  case USB::IOCTL_USBV0_SUSPENDDEV:
  case USB::IOCTL_USBV0_RESUMEDEV:
    // Unimplemented because libusb doesn't do power management.
    return IPCReply(IPC_SUCCESS);
  case USB::IOCTL_USBV0_RESET_DEVICE:
    TriggerHook(m_removal_hooks, device_id, IPC_SUCCESS);
    return IPCReply(IPC_SUCCESS);
  default:
    return IPCReply(IPC_EINVAL);
  }
}

std::optional<IPCReply> OH0::DeviceIOCtlV(const u64 device_id, const IOCtlVRequest& request)
{
  const auto device = GetDeviceById(device_id);
  if (!device)
    return IPCReply(IPC_ENOENT);

  switch (request.request)
  {
  case USB::IOCTLV_USBV0_CTRLMSG:
  case USB::IOCTLV_USBV0_BLKMSG:
  case USB::IOCTLV_USBV0_LBLKMSG:
  case USB::IOCTLV_USBV0_INTRMSG:
  case USB::IOCTLV_USBV0_ISOMSG:
    return HandleTransfer(device, request.request,
                          [&, this]() { return SubmitTransfer(*device, request); });
  case USB::IOCTLV_USBV0_UNKNOWN_32:
    request.DumpUnknown(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_USB);
    return IPCReply(IPC_SUCCESS);
  default:
    return IPCReply(IPC_EINVAL);
  }
}

s32 OH0::SubmitTransfer(USB::Device& device, const IOCtlVRequest& ioctlv)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  switch (ioctlv.request)
  {
  case USB::IOCTLV_USBV0_CTRLMSG:
    if (!ioctlv.HasNumberOfValidVectors(6, 1) ||
        Common::swap16(memory.Read_U16(ioctlv.in_vectors[4].address)) != ioctlv.io_vectors[0].size)
      return IPC_EINVAL;
    return device.SubmitTransfer(
        std::make_unique<USB::V0CtrlMessage>(GetEmulationKernel(), ioctlv));

  case USB::IOCTLV_USBV0_BLKMSG:
  case USB::IOCTLV_USBV0_LBLKMSG:
    if (!ioctlv.HasNumberOfValidVectors(2, 1) ||
        memory.Read_U16(ioctlv.in_vectors[1].address) != ioctlv.io_vectors[0].size)
      return IPC_EINVAL;
    return device.SubmitTransfer(std::make_unique<USB::V0BulkMessage>(
        GetEmulationKernel(), ioctlv, ioctlv.request == USB::IOCTLV_USBV0_LBLKMSG));

  case USB::IOCTLV_USBV0_INTRMSG:
    if (!ioctlv.HasNumberOfValidVectors(2, 1) ||
        memory.Read_U16(ioctlv.in_vectors[1].address) != ioctlv.io_vectors[0].size)
      return IPC_EINVAL;
    return device.SubmitTransfer(
        std::make_unique<USB::V0IntrMessage>(GetEmulationKernel(), ioctlv));

  case USB::IOCTLV_USBV0_ISOMSG:
    if (!ioctlv.HasNumberOfValidVectors(3, 2))
      return IPC_EINVAL;
    return device.SubmitTransfer(std::make_unique<USB::V0IsoMessage>(GetEmulationKernel(), ioctlv));

  default:
    return IPC_EINVAL;
  }
}
}  // namespace IOS::HLE
