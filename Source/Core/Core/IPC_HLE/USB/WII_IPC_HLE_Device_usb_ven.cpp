// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/USB/Common.h"
#include "Core/IPC_HLE/USB/USBV5.h"
#include "Core/IPC_HLE/USB/WII_IPC_HLE_Device_usb_ven.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

CWII_IPC_HLE_Device_usb_ven::CWII_IPC_HLE_Device_usb_ven(u32 dev_id, const std::string& dev_name)
    : CWII_IPC_HLE_Device_usb_host(dev_id, dev_name)
{
}

static const std::map<u32, unsigned int> s_expected_num_parameters = {
    {USBV5_IOCTL_CTRLMSG, 2},
    {USBV5_IOCTL_INTRMSG, 2},
    {USBV5_IOCTL_BULKMSG, 2},
    {USBV5_IOCTL_ISOMSG, 4},
};

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::IOCtlV(u32 command_address)
{
  const SIOCtlVBuffer cmd_buffer(command_address);
  switch (cmd_buffer.Parameter)
  {
  case USBV5_IOCTL_CTRLMSG:
  case USBV5_IOCTL_INTRMSG:
  case USBV5_IOCTL_BULKMSG:
  case USBV5_IOCTL_ISOMSG:
  {
    if (cmd_buffer.NumberInBuffer + cmd_buffer.NumberPayloadBuffer !=
        s_expected_num_parameters.at(cmd_buffer.Parameter))
    {
      Memory::Write_U32(FS_EINVAL, command_address + 4);
      return GetDefaultReply();
    }

    auto device = GetDeviceById(std::make_unique<USBV5TransferCommand>(cmd_buffer)->device_id);
    if (Core::g_want_determinism || !device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    device->AttachDevice();

    int ret = -1;
    if (cmd_buffer.Parameter == USBV5_IOCTL_CTRLMSG)
      ret = device->SubmitTransfer(std::make_unique<USBV5CtrlMessage>(cmd_buffer));
    else if (cmd_buffer.Parameter == USBV5_IOCTL_INTRMSG)
      ret = device->SubmitTransfer(std::make_unique<USBV5IntrMessage>(cmd_buffer));
    else if (cmd_buffer.Parameter == USBV5_IOCTL_BULKMSG)
      ret = device->SubmitTransfer(std::make_unique<USBV5BulkMessage>(cmd_buffer));
    else if (cmd_buffer.Parameter == USBV5_IOCTL_ISOMSG)
      ret = device->SubmitTransfer(std::make_unique<USBV5IsoMessage>(cmd_buffer));

    if (ret < 0)
    {
      ERROR_LOG(WII_IPC_USB, "[%x] Failed to submit transfer (IOCtlV %d): %s", device->GetId(),
                cmd_buffer.Parameter, device->GetErrorName(ret).c_str());
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    Memory::Write_U32(FS_SUCCESS, command_address + 4);
    return GetNoReply();
  }
  default:
    ERROR_LOG(WII_IPC_USB, "Unknown IOCtlV: %x", cmd_buffer.Parameter);
    Memory::Write_U32(FS_EINVAL, command_address + 4);
    return GetDefaultReply();
  }
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::IOCtl(const u32 command_address)
{
  const IOCtlBuffer cmd_buffer(command_address);
  INFO_LOG(WII_IPC_USB, "/dev/usb/ven - IOCtl %d", cmd_buffer.m_request);
  bool send_reply = false;
  switch (cmd_buffer.m_request)
  {
  case USBV5_IOCTL_GETVERSION:
    Memory::Write_U32(VERSION, cmd_buffer.m_out_buffer_addr);
    send_reply = true;
    break;

  case USBV5_IOCTL_GETDEVICECHANGE:
  {
    m_devicechange_hook_address = command_address;
    // On the first call, the reply is sent immediately (instead of on device insertion/removal)
    if (m_devicechange_first_call)
    {
      TriggerDeviceChangeReply();
      m_devicechange_first_call = false;
    }
    else if (!Core::g_want_determinism)
    {
      m_ready_to_trigger_hooks.Set();
    }
    break;
  }

  case USBV5_IOCTL_SHUTDOWN:
  {
    if (m_devicechange_hook_address != 0)
    {
      Memory::Write_U32(-1, m_devicechange_hook_address);
      WII_IPC_HLE_Interface::EnqueueAsyncReply(m_devicechange_hook_address);
      m_devicechange_hook_address = 0;
    }
    send_reply = true;
    break;
  }

  case USBV5_IOCTL_GETDEVPARAMS:
  {
    const s32 device_id = Memory::Read_U32(cmd_buffer.m_in_buffer_addr);
    auto device = GetDeviceById(device_id);
    if (Core::g_want_determinism || !device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    const std::vector<u8> descriptors =
        device->GetIOSDescriptors(cmd_buffer.m_out_buffer_size - 20);
    _assert_(descriptors.size() == cmd_buffer.m_out_buffer_size - 20);
    // The first 20 bytes are copied over from the in buffer.
    Memory::CopyToEmu(cmd_buffer.m_out_buffer_addr, Memory::GetPointer(cmd_buffer.m_in_buffer_addr),
                      20);
    Memory::CopyToEmu(cmd_buffer.m_out_buffer_addr + 20, descriptors.data(), descriptors.size());
    send_reply = true;
    break;
  }

  case USBV5_IOCTL_ATTACHFINISH:
  {
    send_reply = true;
    break;
  }

  case USBV5_IOCTL_SETALTERNATE:
  {
    const s32* buf = reinterpret_cast<s32*>(Memory::GetPointer(cmd_buffer.m_in_buffer_addr));
    const s32 device_id = Common::swap32(buf[0]);
    const u8 alt_setting = buf[2];

    auto device = GetDeviceById(device_id);
    if (Core::g_want_determinism || !device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    device->AttachDevice();
    device->SetAltSetting(alt_setting);
    send_reply = true;
    break;
  }

  case USBV5_IOCTL_SUSPEND_RESUME:
  {
    const s32* buf = reinterpret_cast<s32*>(Memory::GetPointer(cmd_buffer.m_in_buffer_addr));
    const s32 device_id = Common::swap32(buf[0]);
    const s32 resumed = Common::swap32(buf[2]);
    auto device = GetDeviceById(device_id);
    if (Core::g_want_determinism || !device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    // Note: this is unimplemented because there's no easy way to do this in a
    // platform-independant way (libusb does not support power management).
    WARN_LOG(WII_IPC_USB, "[%x] Ignoring USBV5_IOCTL_SUSPEND_RESUME (%s)", device_id,
             resumed == 0 ? "suspend" : "resume");
    send_reply = true;
    break;
  }

  case USBV5_IOCTL_CANCELENDPOINT:
  {
    const s32* buf = reinterpret_cast<s32*>(Memory::GetPointer(cmd_buffer.m_in_buffer_addr));
    const s32 device_id = Common::swap32(buf[0]);
    const u8 endpoint = buf[2];
    auto device = GetDeviceById(device_id);
    if (Core::g_want_determinism || !device)
    {
      Memory::Write_U32(FS_ENOENT, command_address + 4);
      return GetDefaultReply();
    }
    device->AttachDevice();
    device->CancelTransfer(endpoint);
    send_reply = true;
    break;
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
    break;
  }

  Memory::Write_U32(0, command_address + 4);
  return send_reply ? GetDefaultReply() : GetNoReply();
}

void CWII_IPC_HLE_Device_usb_ven::DoState(PointerWrap& p)
{
  CWII_IPC_HLE_Device_usb_host::DoState(p);
  p.Do(m_devicechange_first_call);
  p.Do(m_devicechange_hook_address);
}

s32 CWII_IPC_HLE_Device_usb_ven::GetUSBDeviceId(libusb_device* device)
{
  // Note: IOS only seems to return a limited number of similar device IDs (most commonly
  // e71e0021 and e71f0021, apparently depending on the port), but this doesn't matter
  // since the ID is simply used to uniquely identify a device.
  s32 id = CWII_IPC_HLE_Device_usb_host::GetUSBDeviceId(device);
  // Make sure all device IDs are negative, as libogc relies on that to determine which
  // device fd to use (VEN or HID). Since this is VEN, all device IDs must be negative.
  if (id > 0)
    id = -id;
  return id;
}

void CWII_IPC_HLE_Device_usb_ven::TriggerDeviceChangeReply()
{
  if (m_devicechange_hook_address == 0)
    return;

  const IOCtlBuffer cmd_buffer(m_devicechange_hook_address);
  std::vector<u8> buffer(cmd_buffer.m_out_buffer_size);
  const u8 number_of_devices = GetIOSDeviceList(&buffer);
  Memory::CopyToEmu(cmd_buffer.m_out_buffer_addr, buffer.data(), buffer.size());

  Memory::Write_U32(number_of_devices, m_devicechange_hook_address + 4);
  WII_IPC_HLE_Interface::EnqueueAsyncReply(m_devicechange_hook_address);
  m_devicechange_hook_address = 0;
  INFO_LOG(WII_IPC_USB, "%d device%s", number_of_devices, number_of_devices == 1 ? "" : "s");
}

u8 CWII_IPC_HLE_Device_usb_ven::GetIOSDeviceList(std::vector<u8>* buffer) const
{
  // Return an empty device list.
  if (Core::g_want_determinism)
    return 0;

  std::lock_guard<std::mutex> lk(m_devices_mutex);

  auto it = buffer->begin();
  u8 num_devices = 0;
  const size_t max_num = buffer->size() / sizeof(IOSDeviceEntry);
  for (const auto& device : m_devices)
  {
    if (num_devices >= max_num)
    {
      WARN_LOG(WII_IPC_USB, "Too many devices (%d ≥ %ld), skipping", num_devices, max_num);
      break;
    }

    IOSDeviceEntry entry;
    entry.device_id = Common::swap32(device.second->GetId());
    entry.vid = Common::swap16(device.second->GetVid());
    entry.pid = Common::swap16(device.second->GetPid());
    entry.token = 0;  // XXX: what is this used for? (seems to be unused…)
                      // IOS always seems to assign 210001 and 210102 as the two first tokens.

    std::memcpy(&*it, &entry, sizeof(entry));
    it += sizeof(entry);
    ++num_devices;
  }
  return num_devices;
}
