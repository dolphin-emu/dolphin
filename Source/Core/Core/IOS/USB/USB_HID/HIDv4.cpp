// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/USB_HID/HIDv4.h"

#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "Common/Align.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/USBV4.h"
#include "Core/System.h"

namespace IOS::HLE
{
USB_HIDv4::USB_HIDv4(EmulationKernel& ios, const std::string& device_name)
    : USBHost(ios, device_name)
{
}

USB_HIDv4::~USB_HIDv4()
{
  m_scan_thread.Stop();
}

std::optional<IPCReply> USB_HIDv4::IOCtl(const IOCtlRequest& request)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  request.Log(GetDeviceName(), Common::Log::LogType::IOS_USB);
  switch (request.request)
  {
  case USB::IOCTL_USBV4_GETVERSION:
    return IPCReply(VERSION);
  case USB::IOCTL_USBV4_GETDEVICECHANGE:
    return GetDeviceChange(request);
  case USB::IOCTL_USBV4_SHUTDOWN:
    return Shutdown(request);
  case USB::IOCTL_USBV4_SET_SUSPEND:
    // Not implemented in IOS
    return IPCReply(IPC_SUCCESS);
  case USB::IOCTL_USBV4_CANCELINTERRUPT:
    return CancelInterrupt(request);
  case USB::IOCTL_USBV4_GET_US_STRING:
  case USB::IOCTL_USBV4_CTRLMSG:
  case USB::IOCTL_USBV4_INTRMSG_IN:
  case USB::IOCTL_USBV4_INTRMSG_OUT:
  {
    if (request.buffer_in == 0 || request.buffer_in_size != 32)
      return IPCReply(IPC_EINVAL);
    const auto device = GetDeviceByIOSID(memory.Read_U32(request.buffer_in + 16));
    if (!device)
      return IPCReply(IPC_ENOENT);
    if (!device->Attach())
      return IPCReply(IPC_EINVAL);
    return HandleTransfer(device, request.request,
                          [&, this]() { return SubmitTransfer(*device, request); });
  }
  default:
    request.DumpUnknown(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_USB);
    return IPCReply(IPC_SUCCESS);
  }
}

IPCReply USB_HIDv4::CancelInterrupt(const IOCtlRequest& request)
{
  if (request.buffer_in == 0 || request.buffer_in_size != 8)
    return IPCReply(IPC_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  auto device = GetDeviceByIOSID(memory.Read_U32(request.buffer_in));
  if (!device)
    return IPCReply(IPC_ENOENT);
  device->CancelTransfer(memory.Read_U8(request.buffer_in + 4));
  return IPCReply(IPC_SUCCESS);
}

std::optional<IPCReply> USB_HIDv4::GetDeviceChange(const IOCtlRequest& request)
{
  std::lock_guard lk{m_devicechange_hook_address_mutex};
  if (request.buffer_out == 0 || request.buffer_out_size != 0x600)
    return IPCReply(IPC_EINVAL);

  m_devicechange_hook_request = std::make_unique<IOCtlRequest>(GetSystem(), request.address);
  // If there are pending changes, the reply is sent immediately (instead of on device
  // insertion/removal).
  if (m_has_pending_changes)
  {
    TriggerDeviceChangeReply();
    m_has_pending_changes = false;
  }
  return std::nullopt;
}

IPCReply USB_HIDv4::Shutdown(const IOCtlRequest& request)
{
  std::lock_guard lk{m_devicechange_hook_address_mutex};
  if (m_devicechange_hook_request != nullptr)
  {
    auto& system = GetSystem();
    auto& memory = system.GetMemory();
    memory.Write_U32(0xffffffff, m_devicechange_hook_request->buffer_out);
    GetEmulationKernel().EnqueueIPCReply(*m_devicechange_hook_request, -1);
    m_devicechange_hook_request.reset();
  }
  return IPCReply(IPC_SUCCESS);
}

s32 USB_HIDv4::SubmitTransfer(USB::Device& device, const IOCtlRequest& request)
{
  switch (request.request)
  {
  case USB::IOCTL_USBV4_CTRLMSG:
    return device.SubmitTransfer(
        std::make_unique<USB::V4CtrlMessage>(GetEmulationKernel(), request));
  case USB::IOCTL_USBV4_GET_US_STRING:
    return device.SubmitTransfer(
        std::make_unique<USB::V4GetUSStringMessage>(GetEmulationKernel(), request));
  case USB::IOCTL_USBV4_INTRMSG_IN:
  case USB::IOCTL_USBV4_INTRMSG_OUT:
    return device.SubmitTransfer(
        std::make_unique<USB::V4IntrMessage>(GetEmulationKernel(), request));
  default:
    return IPC_EINVAL;
  }
}

void USB_HIDv4::DoState(PointerWrap& p)
{
  p.Do(m_has_pending_changes);
  u32 hook_address = m_devicechange_hook_request ? m_devicechange_hook_request->address : 0;
  p.Do(hook_address);
  if (hook_address != 0)
  {
    m_devicechange_hook_request = std::make_unique<IOCtlRequest>(GetSystem(), hook_address);
  }
  else
  {
    m_devicechange_hook_request.reset();
  }

  p.Do(m_ios_ids);
  p.Do(m_device_ids);

  USBHost::DoState(p);
}

std::shared_ptr<USB::Device> USB_HIDv4::GetDeviceByIOSID(const s32 ios_id) const
{
  std::lock_guard lk{m_id_map_mutex};
  const auto iterator = m_ios_ids.find(ios_id);
  if (iterator == m_ios_ids.cend())
    return nullptr;
  return GetDeviceById(iterator->second);
}

void USB_HIDv4::OnDeviceChange(ChangeEvent event, std::shared_ptr<USB::Device> device)
{
  {
    std::lock_guard id_map_lock{m_id_map_mutex};
    if (event == ChangeEvent::Inserted)
    {
      s32 new_id = 0;
      while (m_ios_ids.contains(new_id))
        ++new_id;
      m_ios_ids[new_id] = device->GetId();
      m_device_ids[device->GetId()] = new_id;
    }
    else if (event == ChangeEvent::Removed && m_device_ids.contains(device->GetId()))
    {
      m_ios_ids.erase(m_device_ids.at(device->GetId()));
      m_device_ids.erase(device->GetId());
    }
  }

  {
    std::lock_guard lk{m_devicechange_hook_address_mutex};
    TriggerDeviceChangeReply();
  }
}

bool USB_HIDv4::ShouldAddDevice(const USB::Device& device) const
{
  return device.HasClass(HID_CLASS);
}

void USB_HIDv4::TriggerDeviceChangeReply()
{
  if (!m_devicechange_hook_request)
  {
    m_has_pending_changes = true;
    return;
  }

  {
    auto& system = GetSystem();
    auto& memory = system.GetMemory();
    std::lock_guard lk(m_devices_mutex);
    const u32 dest = m_devicechange_hook_request->buffer_out;
    u32 offset = 0;
    for (const auto& device : m_devices)
    {
      const std::vector<u8> device_section = GetDeviceEntry(*device.second.get());
      if (offset + device_section.size() > m_devicechange_hook_request->buffer_out_size - 1)
      {
        WARN_LOG_FMT(IOS_USB, "Too many devices connected, skipping");
        break;
      }
      memory.CopyToEmu(dest + offset, device_section.data(), device_section.size());
      offset += Common::AlignUp(static_cast<u32>(device_section.size()), 4);
    }
    // IOS writes 0xffffffff to the buffer when there are no more devices
    memory.Write_U32(0xffffffff, dest + offset);
  }

  GetEmulationKernel().EnqueueIPCReply(*m_devicechange_hook_request, IPC_SUCCESS, 0,
                                       CoreTiming::FromThread::ANY);
  m_devicechange_hook_request.reset();
}

template <typename T>
static void CopyDescriptorToBuffer(std::vector<u8>* buffer, T descriptor)
{
  const size_t size = sizeof(descriptor);
  descriptor.Swap();
  buffer->insert(buffer->end(), reinterpret_cast<const u8*>(&descriptor),
                 reinterpret_cast<const u8*>(&descriptor) + size);
  constexpr size_t number_of_padding_bytes = Common::AlignUp(size, 4) - size;
  buffer->insert(buffer->end(), number_of_padding_bytes, 0);
}

static std::vector<u8> GetDescriptors(const USB::Device& device)
{
  std::vector<u8> buffer;

  CopyDescriptorToBuffer(&buffer, device.GetDeviceDescriptor());
  const auto configurations = device.GetConfigurations();
  for (size_t c = 0; c < configurations.size(); ++c)
  {
    CopyDescriptorToBuffer(&buffer, configurations[c]);
    const auto interfaces = device.GetInterfaces(static_cast<u8>(c));
    for (size_t i = interfaces.size(); i-- > 0;)
    {
      CopyDescriptorToBuffer(&buffer, interfaces[i]);
      for (const auto& endpoint_descriptor : device.GetEndpoints(
               static_cast<u8>(c), interfaces[i].bInterfaceNumber, interfaces[i].bAlternateSetting))
      {
        CopyDescriptorToBuffer(&buffer, endpoint_descriptor);
      }
    }
  }
  return buffer;
}

std::vector<u8> USB_HIDv4::GetDeviceEntry(const USB::Device& device) const
{
  std::lock_guard id_map_lock{m_id_map_mutex};

  // The structure for a device section is as follows:
  //   0-4 bytes: total size of the device data, including the size and the device ID
  //   4-8 bytes: device ID
  //   the rest of the buffer is device descriptors data
  std::vector<u8> entry(8);
  const std::vector<u8> descriptors = GetDescriptors(device);
  const u32 entry_size = Common::swap32(static_cast<u32>(entry.size() + descriptors.size()));
  const u32 ios_device_id = Common::swap32(m_device_ids.at(device.GetId()));
  std::memcpy(entry.data(), &entry_size, sizeof(entry_size));
  std::memcpy(entry.data() + 4, &ios_device_id, sizeof(ios_device_id));
  entry.insert(entry.end(), descriptors.begin(), descriptors.end());

  return entry;
}
}  // namespace IOS::HLE
