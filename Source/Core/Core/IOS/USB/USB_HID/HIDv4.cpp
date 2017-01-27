// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <utility>

#include "Common/Align.h"
#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/USBV4.h"
#include "Core/IOS/USB/USB_HID/HIDv4.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
USB_HIDv4::USB_HIDv4(u32 device_id, const std::string& device_name)
    : USBHost(device_id, device_name)
{
}

USB_HIDv4::~USB_HIDv4()
{
  StopThreads();
}

static int SubmitTransfer(std::shared_ptr<USB::Device> device, const IOCtlRequest& request)
{
  switch (request.request)
  {
  case USB::IOCTL_USBV4_CTRLMSG:
    return device->SubmitTransfer(std::make_unique<USB::V4CtrlMessage>(request));
  case USB::IOCTL_USBV4_GET_US_STRING:
    return device->SubmitTransfer(std::make_unique<USB::V4GetUSStringMessage>(request));
  case USB::IOCTL_USBV4_INTRMSG_IN:
  case USB::IOCTL_USBV4_INTRMSG_OUT:
    return device->SubmitTransfer(std::make_unique<USB::V4IntrMessage>(request));
  default:
    return -1;
  }
}

IPCCommandResult USB_HIDv4::IOCtl(const IOCtlRequest& request)
{
  request.Log(GetDeviceName(), LogTypes::IOS_USB);
  switch (request.request)
  {
  case USB::IOCTL_USBV4_GETVERSION:
    return GetDefaultReply(VERSION);

  case USB::IOCTL_USBV4_GETDEVICECHANGE:
  {
    std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
    if (request.buffer_out == 0 || request.buffer_out_size != 0x600)
      return GetDefaultReply(IPC_EINVAL);

    m_devicechange_hook_request = std::make_unique<IOCtlRequest>(request.address);
    // On the first call, the reply is sent immediately (instead of on device insertion/removal)
    if (m_devicechange_first_call)
    {
      TriggerDeviceChangeReply();
      m_devicechange_first_call = false;
    }
    return GetNoReply();
  }

  case USB::IOCTL_USBV4_SHUTDOWN:
  {
    std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
    if (m_devicechange_hook_request != 0)
    {
      Memory::Write_U32(0xffffffff, m_devicechange_hook_request->buffer_out);
      EnqueueReply(*m_devicechange_hook_request, -1);
      m_devicechange_hook_request.reset();
    }
    return GetDefaultReply(IPC_SUCCESS);
  }

  case USB::IOCTL_USBV4_GET_US_STRING:
  case USB::IOCTL_USBV4_CTRLMSG:
  case USB::IOCTL_USBV4_INTRMSG_IN:
  case USB::IOCTL_USBV4_INTRMSG_OUT:
  {
    auto device = GetDeviceByIOSID(Memory::Read_U32(request.buffer_in + 16));
    if (!device)
      return GetDefaultReply(IPC_ENOENT);

    if (!device->Attach())
      return GetDefaultReply(-1);
    const s32 ret = SubmitTransfer(device, request);
    if (ret < 0)
    {
      ERROR_LOG(IOS_USB, "[%04x:%04x] Failed to submit transfer (IOCtlV %u): %s", device->GetVid(),
                device->GetPid(), request.request, device->GetErrorName(ret).c_str());
    }
    return GetNoReply();
  }

  case USB::IOCTL_USBV4_SET_SUSPEND:
    // Not implemented in IOS
    return GetDefaultReply(IPC_SUCCESS);

  case USB::IOCTL_USBV4_CANCELINTERRUPT:
  {
    if (request.buffer_in == 0 || request.buffer_in_size != 8)
      return GetDefaultReply(IPC_EINVAL);

    auto device = GetDeviceByIOSID(Memory::Read_U32(request.buffer_in));
    if (!device)
      return GetDefaultReply(IPC_ENOENT);
    device->CancelTransfer(Memory::Read_U8(request.buffer_in + 4));
    return GetDefaultReply(IPC_SUCCESS);
  }

  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_USB);
    return GetDefaultReply(IPC_SUCCESS);
  }
}

void USB_HIDv4::DoState(PointerWrap& p)
{
  p.Do(m_devicechange_first_call);
  u32 hook_address = m_devicechange_hook_request ? m_devicechange_hook_request->address : 0;
  p.Do(hook_address);
  if (hook_address != 0)
    m_devicechange_hook_request = std::make_unique<IOCtlRequest>(hook_address);

  p.Do(m_ios_ids_to_device_ids_map);
  p.Do(m_device_ids_to_ios_ids_map);

  USBHost::DoState(p);
}

std::shared_ptr<USB::Device> USB_HIDv4::GetDeviceByIOSID(const s32 ios_id) const
{
  std::lock_guard<std::mutex> lk{m_id_map_mutex};
  if (m_ios_ids_to_device_ids_map.count(ios_id) == 0)
    return nullptr;
  return GetDeviceById(m_ios_ids_to_device_ids_map.at(ios_id));
}

void USB_HIDv4::OnDeviceChange(ChangeEvent event, std::shared_ptr<USB::Device> device)
{
  {
    std::lock_guard<std::mutex> id_map_lock{m_id_map_mutex};
    if (event == ChangeEvent::Inserted)
    {
      s32 new_id = 0;
      while (m_ios_ids_to_device_ids_map.count(new_id) != 0)
        ++new_id;
      m_ios_ids_to_device_ids_map.emplace(new_id, device->GetId());
      m_device_ids_to_ios_ids_map.emplace(device->GetId(), new_id);
    }
    else if (event == ChangeEvent::Removed && m_device_ids_to_ios_ids_map.count(device->GetId()))
    {
      m_ios_ids_to_device_ids_map.erase(m_device_ids_to_ios_ids_map.at(device->GetId()));
      m_device_ids_to_ios_ids_map.erase(device->GetId());
    }
  }

  {
    std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
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
    return;

  const u32 dest = m_devicechange_hook_request->buffer_out;
  {
    std::lock_guard<std::mutex> lk(m_devices_mutex);
    unsigned int offset = 0;
    for (const auto& device : m_devices)
    {
      const std::vector<u32> device_section = GetDeviceEntry(*device.second.get());
      if (offset + device_section.size() + 1 > m_devicechange_hook_request->buffer_out_size)
      {
        WARN_LOG(IOS_USB, "Too many devices connected, skipping");
        break;
      }
      Memory::CopyToEmu(dest + offset, device_section.data(), device_section.size() * sizeof(u32));
      offset += Common::AlignUp(static_cast<unsigned int>(device_section.size() * sizeof(u32)), 4);
    }
    // IOS writes 0xffffffff to the buffer when there are no more devices
    Memory::Write_U32(0xffffffff, dest + offset);
  }

  EnqueueReply(*m_devicechange_hook_request, IPC_SUCCESS, 0, CoreTiming::FromThread::ANY);
  m_devicechange_hook_request.reset();
}

std::vector<u32> USB_HIDv4::GetDeviceEntry(USB::Device& device) const
{
  std::lock_guard<std::mutex> id_map_lock{m_id_map_mutex};
  const std::vector<u8> descriptors = device.GetDescriptorsUSBV4();

  // The structure for a device section is as follows:
  //   buffer[0] = total size of the device data, including the size and the device ID
  //   buffer[1] = device ID
  //   the rest of the buffer is device descriptors data
  std::vector<u32> buffer(2 + descriptors.size() / sizeof(u32));
  buffer[0] = Common::swap32(static_cast<u32>(buffer.size() * sizeof(u32)));
  buffer[1] = Common::swap32(m_device_ids_to_ios_ids_map.at(device.GetId()));
  std::memcpy(&buffer[2], descriptors.data(), descriptors.size());

  return buffer;
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
