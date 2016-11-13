// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/USBV5.h"
#include "Core/IOS/USB/USB_VEN/VEN.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
USB_VEN::USB_VEN(u32 device_id, const std::string& device_name) : USBHost(device_id, device_name)
{
}

USB_VEN::~USB_VEN()
{
  StopThreads();
}

ReturnCode USB_VEN::Open(const OpenRequest& request)
{
  // TODO: get this from WII_IPC_HLE_Interface once we have a proper version system
  const u16 ios_major_version = Memory::Read_U16(0x3140);
  if (ios_major_version != 57 && ios_major_version != 58 && ios_major_version != 59)
    return IPC_ENOENT;
  return USBHost::Open(request);
}

static int SubmitTransfer(std::shared_ptr<USB::Device> device, const IOCtlVRequest& ioctlv)
{
  switch (ioctlv.request)
  {
  case USB::IOCTLV_USBV5_CTRLMSG:
    return device->SubmitTransfer(std::make_unique<USB::V5CtrlMessage>(ioctlv));
  case USB::IOCTLV_USBV5_INTRMSG:
    return device->SubmitTransfer(std::make_unique<USB::V5IntrMessage>(ioctlv));
  case USB::IOCTLV_USBV5_BULKMSG:
    return device->SubmitTransfer(std::make_unique<USB::V5BulkMessage>(ioctlv));
  case USB::IOCTLV_USBV5_ISOMSG:
    return device->SubmitTransfer(std::make_unique<USB::V5IsoMessage>(ioctlv));
  default:
    return -1;
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

    const s32 device_id = Memory::Read_U32(request.in_vectors[0].address);
    auto device = GetDeviceByIOSID(device_id);
    if (!device)
      return GetDefaultReply(IPC_ENOENT);

    if (!device->Attach(GetInterfaceNumber(device_id)))
      return GetDefaultReply(-1);
    const s32 ret = SubmitTransfer(device, request);
    if (ret < 0)
    {
      ERROR_LOG(IOS_USB, "[%04x:%04x] Failed to submit transfer (IOCtlV %u): %s", device->GetVid(),
                device->GetPid(), request.request, device->GetErrorName(ret).c_str());
    }
    // SubmitTransfer is responsible for replying to the IOS request.
    return GetNoReply();
  }
  default:
    return GetDefaultReply(IPC_EINVAL);
  }
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
  {
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

  case USB::IOCTL_USBV5_SHUTDOWN:
  {
    if (request.buffer_in != 0 || request.buffer_in_size != 0 || request.buffer_out != 0 ||
        request.buffer_out_size != 0)
    {
      return GetDefaultReply(IPC_EINVAL);
    }

    std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
    if (m_devicechange_hook_request)
    {
      EnqueueReply(*m_devicechange_hook_request, IPC_SUCCESS);
      m_devicechange_hook_request.reset();
    }
    return GetDefaultReply(IPC_SUCCESS);
  }

  case USB::IOCTL_USBV5_GETDEVPARAMS:
  {
    const s32 device_id = Memory::Read_U32(request.buffer_in);
    auto device = GetDeviceByIOSID(device_id);
    if (!device)
      return GetDefaultReply(IPC_ENOENT);
    if (request.buffer_in_size != 0x20 || request.buffer_out_size != 0xc0)
      return GetDefaultReply(IPC_EINVAL);

    const u8 alt_setting = Memory::Read_U8(request.buffer_in + 8);
    std::vector<u8> descriptors =
        device->GetDescriptorsUSBV5(GetInterfaceNumber(device_id), alt_setting);
    if (descriptors.empty())
      return GetDefaultReply(IPC_ENOENT);
    descriptors.resize(request.buffer_out_size - 20);
    if (descriptors.size() > request.buffer_out_size - 20)
      WARN_LOG(IOS_USB, "Buffer is too large. Only the first 172 bytes will be copied.");
    Memory::Memset(request.buffer_out, 0, request.buffer_out_size);
    Memory::Write_U32(device_id, request.buffer_out);
    Memory::Write_U32(1, request.buffer_out + 4);
    Memory::CopyToEmu(request.buffer_out + 20, descriptors.data(), descriptors.size());
    return GetDefaultReply(IPC_SUCCESS);
  }

  case USB::IOCTL_USBV5_ATTACHFINISH:
    return GetDefaultReply(IPC_SUCCESS);

  case USB::IOCTL_USBV5_SETALTERNATE:
  {
    const s32 device_id = Memory::Read_U32(request.buffer_in);
    const u8 alt_setting = Memory::Read_U8(request.buffer_in + 2 * sizeof(s32));
    auto device = GetDeviceByIOSID(device_id);
    if (!device)
      return GetDefaultReply(IPC_ENOENT);

    if (!device->Attach(GetInterfaceNumber(device_id)))
      return GetDefaultReply(-1);
    const bool success = device->SetAltSetting(alt_setting) == 0;
    return GetDefaultReply(success ? IPC_SUCCESS : IPC_EINVAL);
  }

  case USB::IOCTL_USBV5_SUSPEND_RESUME:
  {
    const s32 device_id = Memory::Read_U32(request.buffer_in);
    const s32 resumed = Memory::Read_U32(request.buffer_in + 2 * sizeof(s32));
    auto device = GetDeviceByIOSID(device_id);
    if (!device)
      return GetDefaultReply(IPC_ENOENT);

    // Note: this is unimplemented because there's no easy way to do this in a
    // platform-independant way (libusb does not support power management).
    INFO_LOG(IOS_USB, "[%04x:%04x %d] Received %s command", device->GetVid(), device->GetPid(),
             GetInterfaceNumber(device_id), resumed == 0 ? "suspend" : "resume");
    return GetDefaultReply(IPC_SUCCESS);
  }

  case USB::IOCTL_USBV5_CANCELENDPOINT:
  {
    const s32 device_id = Memory::Read_U32(request.buffer_in);
    const u8 endpoint = static_cast<u8>(Memory::Read_U32(request.buffer_in + 2 * sizeof(s32)));
    auto device = GetDeviceByIOSID(device_id);
    if (!device)
      return GetDefaultReply(IPC_ENOENT);

    return GetDefaultReply(device->CancelTransfer(endpoint));
  }

  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_USB, LogTypes::LERROR);
    return GetDefaultReply(IPC_SUCCESS);
  }
}

void USB_VEN::DoState(PointerWrap& p)
{
  p.Do(m_devicechange_first_call);
  u32 hook_address = m_devicechange_hook_request ? m_devicechange_hook_request->address : 0;
  p.Do(hook_address);
  if (hook_address != 0)
    m_devicechange_hook_request = std::make_unique<IOCtlRequest>(hook_address);

  p.Do(m_device_number);
  p.Do(m_ios_ids_to_device_ids_map);
  p.Do(m_device_ids_to_ios_ids_map);
  USBHost::DoState(p);
}

std::shared_ptr<USB::Device> USB_VEN::GetDeviceByIOSID(const s32 ios_id) const
{
  std::lock_guard<std::mutex> lk{m_id_map_mutex};
  if (m_ios_ids_to_device_ids_map.count(ios_id) == 0)
    return nullptr;
  return GetDeviceById(m_ios_ids_to_device_ids_map.at(ios_id));
}

u8 USB_VEN::GetInterfaceNumber(const s32 ios_id) const
{
  const s32 id = Common::swap32(ios_id);
  DeviceID device_id;
  std::memcpy(&device_id, &id, sizeof(id));
  return device_id.interface_plus_1e - 0x1e;
}

void USB_VEN::OnDeviceChange(const ChangeEvent event, std::shared_ptr<USB::Device> device)
{
  std::lock_guard<std::mutex> id_map_lock{m_id_map_mutex};
  if (event == ChangeEvent::Inserted)
  {
    for (const auto& interface : device->GetInterfaces(0))
    {
      if (interface.bAlternateSetting != 0)
        continue;

      // IOS's device list contains entries of the form:
      //   e7 XX 00 YY   VV VV PP PP   00  YY DD  AA
      //   ^^^^^^^^^^^   ^^^^^ ^^^^^   ^^  ^^^^^  ^^
      //    Device ID     VID   PID    ?? See ID  Number of alt settings
      //
      // e7 can sometimes and randomly be e6 instead, for no obvious reason.
      // XX is 1e (for a device plugged in to the left port) + DD (interface number).
      // YY is a counter that starts at 21 and is incremented on every device change.
      // DD is the interface number (since VEN exposes each interface as a separate device).
      // AA is number of alternate settings on this interface.
      DeviceID id;
      id.unknown = 0xe7;
      id.interface_plus_1e = interface.bInterfaceNumber + 0x1e;
      id.zero = 0x00;
      id.counter = m_device_number;

      s32 ios_device_id = 0;
      std::memcpy(&ios_device_id, &id, sizeof(id));
      ios_device_id = Common::swap32(ios_device_id);
      m_ios_ids_to_device_ids_map[ios_device_id] = device->GetId();
      m_device_ids_to_ios_ids_map[device->GetId()].insert(ios_device_id);
    }
  }
  else if (event == ChangeEvent::Removed)
  {
    for (const s32 ios_id : m_device_ids_to_ios_ids_map[device->GetId()])
      m_ios_ids_to_device_ids_map.erase(ios_id);
    m_device_ids_to_ios_ids_map.erase(device->GetId());
  }
}

void USB_VEN::OnDeviceChangeEnd()
{
  std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
  TriggerDeviceChangeReply();
  ++m_device_number;
}

void USB_VEN::TriggerDeviceChangeReply()
{
  if (!m_devicechange_hook_request)
    return;

  std::vector<u8> buffer(m_devicechange_hook_request->buffer_out_size);
  const u8 number_of_devices = GetIOSDeviceList(&buffer);
  Memory::CopyToEmu(m_devicechange_hook_request->buffer_out, buffer.data(), buffer.size());

  EnqueueReply(*m_devicechange_hook_request, number_of_devices, 0, CoreTiming::FromThread::ANY);
  m_devicechange_hook_request.reset();
  INFO_LOG(IOS_USB, "/dev/usb/ven: %d device%s connected (including interfaces)", number_of_devices,
           number_of_devices == 1 ? "" : "s");
}

u8 USB_VEN::GetIOSDeviceList(std::vector<u8>* buffer) const
{
  std::lock_guard<std::mutex> id_map_lock{m_id_map_mutex};

  auto it = buffer->begin();
  u8 num_devices = 0;
  const size_t max_num = buffer->size() / sizeof(DeviceEntry);
  for (const auto& ios_device : m_ios_ids_to_device_ids_map)
  {
    if (num_devices >= max_num)
    {
      WARN_LOG(IOS_USB, "Too many devices (%d ≥ %zu), skipping", num_devices, max_num);
      break;
    }

    const s32 ios_device_id = ios_device.first;
    const auto device = GetDeviceById(m_ios_ids_to_device_ids_map.at(ios_device_id));
    if (!device)
      continue;
    const u8 interface_number = GetInterfaceNumber(ios_device_id);

    DeviceEntry entry;
    entry.device_id = Common::swap32(ios_device_id);
    entry.vid = Common::swap16(device->GetVid());
    entry.pid = Common::swap16(device->GetPid());
    entry.unknown = 0x00;
    entry.device_number = ios_device_id & 0xff;
    entry.interface_number = interface_number;
    entry.num_altsettings = device->GetNumberOfAltSettings(interface_number);

    std::memcpy(&*it, &entry, sizeof(entry));
    it += sizeof(entry);
    ++num_devices;
  }
  return num_devices;
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
