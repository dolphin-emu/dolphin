// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/USB/WII_IPC_HLE_Device_usb_hid_v4.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/USB/USBV4.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

CWII_IPC_HLE_Device_usb_hid_v4::CWII_IPC_HLE_Device_usb_hid_v4(u32 dev_id,
                                                               const std::string& dev_name)
    : CWII_IPC_HLE_Device_usb_host(dev_id, dev_name)
{
  m_ready_to_trigger_hooks.Set();
}

CWII_IPC_HLE_Device_usb_hid_v4::~CWII_IPC_HLE_Device_usb_hid_v4()
{
  StopThreads();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_hid_v4::IOCtlV(const u32 command_address)
{
  Memory::Write_U32(FS_EINVAL, command_address + 4);
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_hid_v4::IOCtl(const u32 command_address)
{
  const IOCtlBuffer cmd_buffer(command_address);
  Memory::Write_U32(FS_SUCCESS, command_address + 4);
  switch (cmd_buffer.m_request)
  {
  case USBV4_IOCTL_GETVERSION:
    Memory::Write_U32(VERSION, command_address + 4);
    return GetDefaultReply();

  case USBV4_IOCTL_GETDEVICECHANGE:
  {
    std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
    if (cmd_buffer.m_out_buffer_size != 0x600)
    {
      Memory::Write_U32(FS_EINVAL, command_address + 4);
      return GetDefaultReply();
    }
    m_devicechange_hook_address = command_address;
    // On the first call, the reply is sent immediately (instead of on device insertion/removal)
    if (m_devicechange_first_call)
    {
      TriggerDeviceChangeReply();
      m_devicechange_first_call = false;
    }
    return GetNoReply();
  }

  case USBV4_IOCTL_SHUTDOWN:
  {
    std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
    if (m_devicechange_hook_address != 0)
    {
      Memory::Write_U32(-1, m_devicechange_hook_address + 4);
      Memory::Write_U32(0xffffffff, m_devicechange_hook_address + 0x18);
      WII_IPC_HLE_Interface::EnqueueReply(m_devicechange_hook_address);
      m_devicechange_hook_address = 0;
    }
    return GetDefaultReply();
  }

  case USBV4_IOCTL_GET_US_STRING:
  case USBV4_IOCTL_CTRLMSG:
  case USBV4_IOCTL_INTRMSG_IN:
  case USBV4_IOCTL_INTRMSG_OUT:
  {
    auto device = GetDeviceByIOSId(std::make_unique<USBV4Request>(cmd_buffer)->device_id);
    if (Core::g_want_determinism || !device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    device->AttachDevice();
    int ret = -1;
    if (cmd_buffer.m_request == USBV4_IOCTL_CTRLMSG)
      ret = device->SubmitTransfer(std::make_unique<USBV4CtrlMessage>(cmd_buffer));
    else if (cmd_buffer.m_request == USBV4_IOCTL_GET_US_STRING)
      ret = device->SubmitTransfer(std::make_unique<USBV4GetUSStringMessage>(cmd_buffer));
    else
      ret = device->SubmitTransfer(std::make_unique<USBV4IntrMessage>(cmd_buffer));

    if (ret < 0)
    {
      ERROR_LOG(WII_IPC_USB, "[%04x:%04x] Failed to submit transfer (IOCtl %d): %s",
                device->GetVid(), device->GetPid(), cmd_buffer.m_request,
                device->GetErrorName(ret).c_str());
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    return GetNoReply();
  }

  case USBV4_IOCTL_SET_SUSPEND:
    // Not implemented in IOS
    return GetDefaultReply();

  case USBV4_IOCTL_CANCELINTERRUPT:
  {
    if (cmd_buffer.m_in_buffer_size != 8)
    {
      Memory::Write_U32(FS_EINVAL, command_address + 4);
      return GetDefaultReply();
    }

    auto device = GetDeviceByIOSId(Memory::Read_U32(cmd_buffer.m_in_buffer_addr));
    if (Core::g_want_determinism || !device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    device->AttachDevice();
    device->CancelTransfer(Memory::Read_U8(cmd_buffer.m_in_buffer_addr + 4));
    return GetDefaultReply();
  }

  default:
    ERROR_LOG(WII_IPC_USB, "Unknown IOCtl: %x", cmd_buffer.m_request);
    ERROR_LOG(WII_IPC_USB, "In\n%s", ArrayToString(Memory::GetPointer(cmd_buffer.m_in_buffer_addr),
                                                   cmd_buffer.m_in_buffer_size)
                                         .c_str());
    ERROR_LOG(WII_IPC_USB, "Out\n%s",
              ArrayToString(Memory::GetPointer(cmd_buffer.m_out_buffer_addr),
                            cmd_buffer.m_out_buffer_size)
                  .c_str());
    return GetNoReply();
  }
}

void CWII_IPC_HLE_Device_usb_hid_v4::DoState(PointerWrap& p)
{
  p.Do(m_devicechange_first_call);
  p.Do(m_devicechange_hook_address);
  p.Do(m_ios_ids_to_device_ids_map);
  p.Do(m_device_ids_to_ios_ids_map);
}

std::shared_ptr<Device> CWII_IPC_HLE_Device_usb_hid_v4::GetDeviceByIOSId(const s32 ios_id) const
{
  std::lock_guard<std::mutex> lk{m_id_map_mutex};
  if (m_ios_ids_to_device_ids_map.count(ios_id) == 0)
    return nullptr;
  return GetDeviceById(m_ios_ids_to_device_ids_map.at(ios_id));
}

void CWII_IPC_HLE_Device_usb_hid_v4::OnDeviceChange(ChangeEvent event, std::shared_ptr<Device> dev)
{
  {
    std::lock_guard<std::mutex> id_map_lock{m_id_map_mutex};
    if (event == ChangeEvent::Inserted)
    {
      s32 new_id = 0;
      while (m_ios_ids_to_device_ids_map.count(new_id) != 0)
        ++new_id;
      m_ios_ids_to_device_ids_map.emplace(new_id, dev->GetId());
      m_device_ids_to_ios_ids_map.emplace(dev->GetId(), new_id);
    }
    else if (event == ChangeEvent::Removed)
    {
      m_ios_ids_to_device_ids_map.erase(m_device_ids_to_ios_ids_map.at(dev->GetId()));
      m_device_ids_to_ios_ids_map.erase(dev->GetId());
    }
  }

  {
    std::lock_guard<std::mutex> lk{m_devicechange_hook_address_mutex};
    TriggerDeviceChangeReply();
  }
}

void CWII_IPC_HLE_Device_usb_hid_v4::TriggerDeviceChangeReply()
{
  if (m_devicechange_hook_address == 0)
    return;

  const IOCtlBuffer cmd_buffer(m_devicechange_hook_address);
  const u32 dest = cmd_buffer.m_out_buffer_addr;
  if (Core::g_want_determinism)
  {
    if (m_devicechange_first_call)
    {
      Memory::Write_U32(0xffffffff, dest);
      Memory::Write_U32(FS_SUCCESS, m_devicechange_hook_address + 4);
      WII_IPC_HLE_Interface::EnqueueReply(m_devicechange_hook_address);
    }
    m_devicechange_hook_address = 0;
    return;
  }

  {
    std::lock_guard<std::mutex> lk(m_devices_mutex);
    int offset = 0;
    for (const auto& device : m_devices)
    {
      if (device.second->GetInterfaceClass() != HID_CLASS)
        continue;

      const std::vector<u32> device_section = GetDeviceEntry(*device.second.get());
      if (offset + device_section.size() + 1 > cmd_buffer.m_out_buffer_size)
      {
        WARN_LOG(WII_IPC_USB, "Too many devices connected, skipping");
        break;
      }
      Memory::CopyToEmu(dest + offset, device_section.data(), device_section.size() * sizeof(u32));
      offset += Align(static_cast<int>(device_section.size() * sizeof(u32)), 4);
    }
    // IOS writes 0xffffffff to the buffer when there are no more devices
    Memory::Write_U32(0xffffffff, dest + offset);
  }

  Memory::Write_U32(FS_SUCCESS, m_devicechange_hook_address + 4);
  WII_IPC_HLE_Interface::EnqueueReply(m_devicechange_hook_address, 0, CoreTiming::FromThread::ANY);
  m_devicechange_hook_address = 0;
}

std::vector<u32> CWII_IPC_HLE_Device_usb_hid_v4::GetDeviceEntry(Device& device) const
{
  std::lock_guard<std::mutex> id_map_lock{m_id_map_mutex};
  const std::vector<u8> descriptors = device.GetIOSDescriptors(Version::USBV4);

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
