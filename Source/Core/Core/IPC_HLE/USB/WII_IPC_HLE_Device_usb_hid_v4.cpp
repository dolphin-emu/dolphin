// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <mutex>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/USB/USBV4.h"
#include "Core/IPC_HLE/USB/WII_IPC_HLE_Device_usb_hid_v4.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

CWII_IPC_HLE_Device_usb_hid_v4::CWII_IPC_HLE_Device_usb_hid_v4(u32 dev_id,
                                                               const std::string& dev_name)
    : CWII_IPC_HLE_Device_usb_host(dev_id, dev_name)
{
  m_ready_to_trigger_hooks.Set();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_hid_v4::IOCtlV(const u32 command_address)
{
  Memory::Write_U32(FS_EINVAL, command_address + 4);
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_hid_v4::IOCtl(const u32 command_address)
{
  const IOCtlBuffer cmd_buffer(command_address);
  Memory::Write_U32(0, command_address + 4);
  bool send_reply = false;
  switch (cmd_buffer.m_request)
  {
  case USBV4_IOCTL_GETVERSION:
    Memory::Write_U32(VERSION, command_address + 4);
    send_reply = true;
    break;

  case USBV4_IOCTL_GETDEVICECHANGE:
  {
    _assert_msg_(WII_IPC_USB, cmd_buffer.m_out_buffer_size == 0x600, "Wrong output buffer size");
    m_devicechange_hook_address = command_address;
    // On the first call, the reply is sent immediately (instead of on device insertion/removal)
    if (m_devicechange_first_call)
    {
      TriggerDeviceChangeReply();
      m_devicechange_first_call = false;
    }
    break;
  }

  case USBV4_IOCTL_SHUTDOWN:
  {
    if (m_devicechange_hook_address != 0)
    {
      Memory::Write_U32(-1, m_devicechange_hook_address + 4);
      Memory::Write_U32(0xffffffff, m_devicechange_hook_address + 0x18);
      WII_IPC_HLE_Interface::EnqueueAsyncReply(m_devicechange_hook_address);
      m_devicechange_hook_address = 0;
    }
    send_reply = true;
    break;
  }

  case USBV4_IOCTL_GET_US_STRING:
  case USBV4_IOCTL_CTRLMSG:
  case USBV4_IOCTL_INTRMSG_IN:
  case USBV4_IOCTL_INTRMSG_OUT:
  {
    auto device = GetDeviceById(std::make_unique<USBV4Request>(cmd_buffer)->device_id);
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
      ERROR_LOG(WII_IPC_USB, "[%x] Failed to submit transfer (IOCtl %d): %s", device->GetId(),
                cmd_buffer.m_request, device->GetErrorName(ret).c_str());
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    return GetNoReply();
  }

  case USBV4_IOCTL_SET_SUSPEND:
    // Not implemented in IOS
    send_reply = true;
    break;

  case USBV4_IOCTL_CANCELINTERRUPT:
    // TODO: figure out what's passed to this ioctl and really cancel an interrupt transfer
    ERROR_LOG(WII_IPC_USB, "Unimplemented IOCtl: USBV4_IOCTL_CANCELINTERRUPT");
    send_reply = true;
  // fallthrough
  default:
    ERROR_LOG(WII_IPC_USB, "Unknown IOCtl: %x", cmd_buffer.m_request);
    ERROR_LOG(WII_IPC_USB, "In\n%s", ArrayToString(Memory::GetPointer(cmd_buffer.m_in_buffer_addr),
                                                   cmd_buffer.m_in_buffer_size)
                                         .c_str());
    ERROR_LOG(WII_IPC_USB, "Out\n%s",
              ArrayToString(Memory::GetPointer(cmd_buffer.m_out_buffer_addr),
                            cmd_buffer.m_out_buffer_size)
                  .c_str());
    break;
  }

  return send_reply ? GetDefaultReply() : GetNoReply();
}

void CWII_IPC_HLE_Device_usb_hid_v4::DoState(PointerWrap& p)
{
  p.Do(m_devicechange_first_call);
  p.Do(m_devicechange_hook_address);
  p.Do(m_device_ids_map);
  p.Do(m_used_ids);
  p.Do(m_next_free_id);
}

s32 CWII_IPC_HLE_Device_usb_hid_v4::GetUSBDeviceId(libusb_device* device)
{
  const s32 id = CWII_IPC_HLE_Device_usb_host::GetUSBDeviceId(device);
  const auto iterator = m_device_ids_map.find(id);
  if (iterator != m_device_ids_map.end())
    return iterator->second;

  // Find a free ID
  s32 new_id = m_next_free_id;
  while (m_used_ids.count(new_id) != 0)
    ++new_id;
  m_device_ids_map.emplace(id, new_id);
  m_next_free_id = new_id + 1;
  return new_id;
}

void CWII_IPC_HLE_Device_usb_hid_v4::OnDeviceChange(ChangeEvent event, std::shared_ptr<Device> dev)
{
  if (Core::g_want_determinism)
    return;
  TriggerDeviceChangeReply();
  if (event == ChangeEvent::Removed)
  {
    m_next_free_id = std::min(dev->GetId(), m_next_free_id);
    m_used_ids.erase(m_device_ids_map[dev->GetId()]);
  }
}

void CWII_IPC_HLE_Device_usb_hid_v4::TriggerDeviceChangeReply()
{
  if (m_devicechange_hook_address == 0)
    return;

  const IOCtlBuffer cmd_buffer(m_devicechange_hook_address);

  {
    std::lock_guard<std::mutex> lk(m_devices_mutex);
    const u32 dest = cmd_buffer.m_out_buffer_addr;
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
      Memory::CopyToEmu(dest + offset, device_section.data(), device_section.size() * 4);
      offset += Align(static_cast<int>(device_section.size() * 4), 4);
    }
    Memory::Write_U32(0xffffffff, dest + offset);
  }

  Memory::Write_U32(FS_SUCCESS, m_devicechange_hook_address + 4);
  WII_IPC_HLE_Interface::EnqueueAsyncReply(m_devicechange_hook_address);
  m_devicechange_hook_address = 0;
}

std::vector<u32> CWII_IPC_HLE_Device_usb_hid_v4::GetDeviceEntry(Device& device) const
{
  // Maximum size for descriptors is 0x5f4, assuming there's only a single device entry
  size_t real_size = 0;
  const std::vector<u8> descriptors = device.GetIOSDescriptors(0x5f4, &real_size, Version::USBV4);

  // The structure for a device section is as follows:
  //   buffer[0] = total size of the device data, including the size and the device ID
  //   buffer[1] = device ID
  //   the rest of the buffer is device descriptors data
  std::vector<u32> buffer(2 + real_size / 4);
  buffer[0] = Common::swap32(static_cast<u32>(buffer.size() * 4));
  buffer[1] = Common::swap32(device.GetId());
  std::memcpy(&buffer[2], descriptors.data(), real_size);

  return buffer;
}
