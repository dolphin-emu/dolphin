// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/USB_VEN/VEN.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/USBV5.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
namespace
{
#pragma pack(push, 1)
struct DeviceID
{
  u8 ipc_address_shifted;
  u8 index;
  u16 number;
};

struct DeviceEntry
{
  DeviceID id;
  u16 vid;
  u16 pid;
  u16 number;
  u8 interface_number;
  u8 num_altsettings;
};
#pragma pack(pop)
}

USB_VEN::USB_VEN(Kernel& ios, const std::string& device_name) : USBHost(ios, device_name)
{
}

USB_VEN::~USB_VEN()
{
  StopThreads();
}

ReturnCode USB_VEN::Open(const OpenRequest& request)
{
  const u32 ios_major_version = m_ios.GetVersion();
  if (ios_major_version != 57 && ios_major_version != 58 && ios_major_version != 59)
    return IPC_ENOENT;
  return USBHost::Open(request);
}

IPCCommandResult USB_VEN::IOCtl(const IOCtlRequest& request)
{
  request.Log(GetDeviceName(), LogTypes::IOS_USB);
  switch (request.request)
  {
  case USB::IOCTL_USBV5_GETVERSION:
    Memory::Write_U32(VERSION, request.buffer_out);
    return GetDefaultReply(IPC_SUCCESS);
  case USB::IOCTL_USBV5_GETDEVICECHANGE:
    return GetDeviceChange(request);
  case USB::IOCTL_USBV5_SHUTDOWN:
    return Shutdown(request);
  case USB::IOCTL_USBV5_GETDEVPARAMS:
    return HandleDeviceIOCtl(request, &USB_VEN::GetDeviceInfo);
  case USB::IOCTL_USBV5_ATTACHFINISH:
    return GetDefaultReply(IPC_SUCCESS);
  case USB::IOCTL_USBV5_SETALTERNATE:
    return HandleDeviceIOCtl(request, &USB_VEN::SetAlternateSetting);
  case USB::IOCTL_USBV5_SUSPEND_RESUME:
    return HandleDeviceIOCtl(request, &USB_VEN::SuspendResume);
  case USB::IOCTL_USBV5_CANCELENDPOINT:
    return HandleDeviceIOCtl(request, &USB_VEN::CancelEndpoint);
  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_USB, LogTypes::LERROR);
    return GetDefaultReply(IPC_SUCCESS);
  }
}

IPCCommandResult USB_VEN::IOCtlV(const IOCtlVRequest& request)
{
  static const std::map<u32, u32> s_num_vectors = {
      {USB::IOCTLV_USBV5_CTRLMSG, 2},
      {USB::IOCTLV_USBV5_INTRMSG, 2},
      {USB::IOCTLV_USBV5_BULKMSG, 2},
      {USB::IOCTLV_USBV5_ISOMSG, 4},
  };

  switch (request.request)
  {
  case USB::IOCTLV_USBV5_CTRLMSG:
  case USB::IOCTLV_USBV5_INTRMSG:
  case USB::IOCTLV_USBV5_BULKMSG:
  case USB::IOCTLV_USBV5_ISOMSG:
  {
    if (request.in_vectors.size() + request.io_vectors.size() != s_num_vectors.at(request.request))
      return GetDefaultReply(IPC_EINVAL);

    std::lock_guard<std::mutex> lock{m_usbv5_devices_mutex};
    USBV5Device* device = GetUSBV5Device(request.in_vectors[0].address);
    if (!device)
      return GetDefaultReply(IPC_EINVAL);
    auto host_device = GetDeviceById(device->host_id);
    host_device->Attach(device->interface_number);
    return HandleTransfer(host_device, request.request,
                          [&, this]() { return SubmitTransfer(*host_device, request); });
  }
  default:
    return GetDefaultReply(IPC_EINVAL);
  }
}

void USB_VEN::DoState(PointerWrap& p)
{
  p.Do(m_devicechange_first_call);
  u32 hook_address = m_devicechange_hook_request ? m_devicechange_hook_request->address : 0;
  p.Do(hook_address);
  if (hook_address != 0)
    m_devicechange_hook_request = std::make_unique<IOCtlRequest>(hook_address);
  else
    m_devicechange_hook_request.reset();

  p.Do(m_usbv5_devices);
  USBHost::DoState(p);
}

USB_VEN::USBV5Device* USB_VEN::GetUSBV5Device(u32 in_buffer)
{
  const u8 index = Memory::Read_U8(in_buffer + offsetof(DeviceID, index));
  const u16 number = Memory::Read_U16(in_buffer + offsetof(DeviceID, number));

  if (index >= m_usbv5_devices.size())
    return nullptr;

  USBV5Device* usbv5_device = &m_usbv5_devices[index];
  if (!usbv5_device->in_use || usbv5_device->number != number)
    return nullptr;

  return usbv5_device;
}

IPCCommandResult USB_VEN::CancelEndpoint(USBV5Device& device, const IOCtlRequest& request)
{
  const u8 endpoint = static_cast<u8>(Memory::Read_U32(request.buffer_in + 8));
  GetDeviceById(device.host_id)->CancelTransfer(endpoint);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult USB_VEN::GetDeviceChange(const IOCtlRequest& request)
{
  if (request.buffer_out_size != 0x180 || m_devicechange_hook_request)
    return GetDefaultReply(IPC_EINVAL);

  std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
  m_devicechange_hook_request = std::make_unique<IOCtlRequest>(request.address);
  // On the first call, the reply is sent immediately (instead of on device insertion/removal)
  if (m_devicechange_first_call)
  {
    TriggerDeviceChangeReply();
    m_devicechange_first_call = false;
  }
  return GetNoReply();
}

IPCCommandResult USB_VEN::GetDeviceInfo(USBV5Device& device, const IOCtlRequest& request)
{
  if (request.buffer_out == 0 || request.buffer_out_size != 0xc0)
    return GetDefaultReply(IPC_EINVAL);

  const auto host_device = GetDeviceById(device.host_id);
  const u8 alt_setting = Memory::Read_U8(request.buffer_in + 8);
  auto descriptors = host_device->GetDescriptorsUSBV5(device.interface_number, alt_setting);
  if (descriptors.empty())
    return GetDefaultReply(IPC_ENOENT);

  descriptors.resize(request.buffer_out_size - 20);
  if (descriptors.size() > request.buffer_out_size - 20)
    WARN_LOG(IOS_USB, "Buffer is too large. Only the first 172 bytes will be copied.");

  Memory::Memset(request.buffer_out, 0, request.buffer_out_size);
  Memory::Write_U32(Memory::Read_U32(request.buffer_in), request.buffer_out);
  Memory::Write_U32(1, request.buffer_out + 4);
  Memory::CopyToEmu(request.buffer_out + 20, descriptors.data(), descriptors.size());

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult USB_VEN::SetAlternateSetting(USBV5Device& device, const IOCtlRequest& request)
{
  const auto host_device = GetDeviceById(device.host_id);
  if (!host_device->Attach(device.interface_number))
    return GetDefaultReply(-1);

  const u8 alt_setting = Memory::Read_U8(request.buffer_in + 2 * sizeof(s32));

  const bool success = host_device->SetAltSetting(alt_setting) == 0;
  return GetDefaultReply(success ? IPC_SUCCESS : IPC_EINVAL);
}

IPCCommandResult USB_VEN::Shutdown(const IOCtlRequest& request)
{
  if (request.buffer_in != 0 || request.buffer_in_size != 0 || request.buffer_out != 0 ||
      request.buffer_out_size != 0)
  {
    return GetDefaultReply(IPC_EINVAL);
  }

  std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
  if (m_devicechange_hook_request)
  {
    m_ios.EnqueueIPCReply(*m_devicechange_hook_request, IPC_SUCCESS);
    m_devicechange_hook_request.reset();
  }
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult USB_VEN::SuspendResume(USBV5Device& device, const IOCtlRequest& request)
{
  const auto host_device = GetDeviceById(device.host_id);
  const s32 resumed = Memory::Read_U32(request.buffer_in + 8);

  // Note: this is unimplemented because there's no easy way to do this in a
  // platform-independant way (libusb does not support power management).
  INFO_LOG(IOS_USB, "[%04x:%04x %d] Received %s command", host_device->GetVid(),
           host_device->GetPid(), device.interface_number, resumed == 0 ? "suspend" : "resume");
  return GetDefaultReply(IPC_SUCCESS);
}

s32 USB_VEN::SubmitTransfer(USB::Device& device, const IOCtlVRequest& ioctlv)
{
  switch (ioctlv.request)
  {
  case USB::IOCTLV_USBV5_CTRLMSG:
    return device.SubmitTransfer(std::make_unique<USB::V5CtrlMessage>(m_ios, ioctlv));
  case USB::IOCTLV_USBV5_INTRMSG:
    return device.SubmitTransfer(std::make_unique<USB::V5IntrMessage>(m_ios, ioctlv));
  case USB::IOCTLV_USBV5_BULKMSG:
    return device.SubmitTransfer(std::make_unique<USB::V5BulkMessage>(m_ios, ioctlv));
  case USB::IOCTLV_USBV5_ISOMSG:
    return device.SubmitTransfer(std::make_unique<USB::V5IsoMessage>(m_ios, ioctlv));
  default:
    return IPC_EINVAL;
  }
}

IPCCommandResult USB_VEN::HandleDeviceIOCtl(const IOCtlRequest& request, Handler handler)
{
  if (request.buffer_in == 0 || request.buffer_in_size != 0x20)
    return GetDefaultReply(IPC_EINVAL);

  std::lock_guard<std::mutex> lock{m_usbv5_devices_mutex};
  USBV5Device* device = GetUSBV5Device(request.buffer_in);
  if (!device)
    return GetDefaultReply(IPC_EINVAL);
  return handler(this, *device, request);
}

void USB_VEN::OnDeviceChange(const ChangeEvent event, std::shared_ptr<USB::Device> device)
{
  std::lock_guard<std::mutex> lock{m_usbv5_devices_mutex};
  const u64 host_device_id = device->GetId();
  if (event == ChangeEvent::Inserted)
  {
    for (const auto& interface : device->GetInterfaces(0))
    {
      if (interface.bAlternateSetting != 0)
        continue;

      auto it = std::find_if(m_usbv5_devices.rbegin(), m_usbv5_devices.rend(),
                             [](const USBV5Device& entry) { return !entry.in_use; });
      if (it == m_usbv5_devices.rend())
        return;

      it->in_use = true;
      it->interface_number = interface.bInterfaceNumber;
      it->number = m_current_device_number;
      it->host_id = host_device_id;
    }
  }
  else if (event == ChangeEvent::Removed)
  {
    for (USBV5Device& entry : m_usbv5_devices)
    {
      if (entry.host_id == host_device_id)
        entry.in_use = false;
    }
  }
}

void USB_VEN::OnDeviceChangeEnd()
{
  std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
  TriggerDeviceChangeReply();
  ++m_current_device_number;
}

void USB_VEN::TriggerDeviceChangeReply()
{
  if (!m_devicechange_hook_request)
    return;

  std::lock_guard<std::mutex> lock{m_usbv5_devices_mutex};
  u8 num_devices = 0;
  for (auto it = m_usbv5_devices.crbegin(); it != m_usbv5_devices.crend(); ++it)
  {
    const USBV5Device& usbv5_device = *it;
    if (!usbv5_device.in_use)
      continue;

    const auto device = GetDeviceById(usbv5_device.host_id);
    if (!device)
      continue;

    DeviceEntry entry;
    // The actual value is static_cast<u8>(hook_internal_ipc_request >> 8).
    // Since we don't actually emulate the IOS kernel and internal IPC,
    // just pretend the value is 0xe7 (most common value according to hwtests).
    entry.id.ipc_address_shifted = 0xe7;
    entry.id.index = static_cast<u8>(std::distance(m_usbv5_devices.cbegin(), it.base()) - 1);
    entry.id.number = Common::swap16(usbv5_device.number);
    entry.vid = Common::swap16(device->GetVid());
    entry.pid = Common::swap16(device->GetPid());
    entry.number = Common::swap16(usbv5_device.number);
    entry.interface_number = usbv5_device.interface_number;
    entry.num_altsettings = device->GetNumberOfAltSettings(entry.interface_number);

    Memory::CopyToEmu(m_devicechange_hook_request->buffer_out + sizeof(entry) * num_devices++,
                      &entry, sizeof(entry));
  }

  m_ios.EnqueueIPCReply(*m_devicechange_hook_request, num_devices, 0, CoreTiming::FromThread::ANY);
  m_devicechange_hook_request.reset();
  INFO_LOG(IOS_USB, "%d USBv5 device(s), including interfaces", num_devices);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
