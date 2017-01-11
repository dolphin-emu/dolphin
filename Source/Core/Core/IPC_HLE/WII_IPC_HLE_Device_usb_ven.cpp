// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_ven.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"

CWII_IPC_HLE_Device_usb_ven::CWII_IPC_HLE_Device_usb_ven(const u32 device_id,
                                                         const std::string& device_name)
    : IWII_IPC_HLE_Device(device_id, device_name)
{
}

CWII_IPC_HLE_Device_usb_ven::~CWII_IPC_HLE_Device_usb_ven()
{
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::IOCtlV(u32 command_address)
{
  SIOCtlVBuffer command_buffer(command_address);

  INFO_LOG(OSHLE, "%s - IOCtlV:", GetDeviceName().c_str());
  INFO_LOG(OSHLE, "  Parameter: 0x%x", command_buffer.Parameter);
  INFO_LOG(OSHLE, "  NumberIn: 0x%08x", command_buffer.NumberInBuffer);
  INFO_LOG(OSHLE, "  NumberOut: 0x%08x", command_buffer.NumberPayloadBuffer);
  INFO_LOG(OSHLE, "  BufferVector: 0x%08x", command_buffer.BufferVector);
  DumpAsync(command_buffer.BufferVector, command_buffer.NumberInBuffer,
            command_buffer.NumberPayloadBuffer);

  Memory::Write_U32(0, command_address + 4);
  return GetNoReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::IOCtl(u32 command_address)
{
  IPCCommandResult reply = GetDefaultReply();
  u32 command = Memory::Read_U32(command_address + 0x0c);
  u32 buffer_in = Memory::Read_U32(command_address + 0x10);
  u32 buffer_in_size = Memory::Read_U32(command_address + 0x14);
  u32 buffer_out = Memory::Read_U32(command_address + 0x18);
  u32 buffer_out_size = Memory::Read_U32(command_address + 0x1c);

  INFO_LOG(OSHLE, "%s - IOCtl: %x", GetDeviceName().c_str(), command);
  INFO_LOG(OSHLE, "%x:%x %x:%x", buffer_in, buffer_in_size, buffer_out, buffer_out_size);

  switch (command)
  {
  case USBV5_IOCTL_GETVERSION:
    Memory::Write_U32(0x50001, buffer_out);
    reply = GetDefaultReply();
    break;

  case USBV5_IOCTL_GETDEVICECHANGE:
  {
    // sent on change
    static bool firstcall = true;
    if (firstcall)
    {
      reply = GetDefaultReply();
      firstcall = false;
    }

    // num devices
    Memory::Write_U32(0, command_address + 4);
    return reply;
  }
  break;

  case USBV5_IOCTL_ATTACHFINISH:
    reply = GetDefaultReply();
    break;

  case USBV5_IOCTL_SUSPEND_RESUME:
    DEBUG_LOG(OSHLE, "Device: %i Resumed: %i", Memory::Read_U32(buffer_in),
              Memory::Read_U32(buffer_in + 4));
    reply = GetDefaultReply();
    break;

  case USBV5_IOCTL_GETDEVPARAMS:
  {
    s32 device = Memory::Read_U32(buffer_in);
    u32 unk = Memory::Read_U32(buffer_in + 4);

    DEBUG_LOG(OSHLE, "USBV5_IOCTL_GETDEVPARAMS device: %i unk: %i", device, unk);

    Memory::Write_U32(0, buffer_out);

    reply = GetDefaultReply();
  }
  break;

  default:
    DEBUG_LOG(OSHLE, "%x:%x %x:%x", buffer_in, buffer_in_size, buffer_out, buffer_out_size);
    break;
  }

  Memory::Write_U32(0, command_address + 4);
  return reply;
}

void CWII_IPC_HLE_Device_usb_ven::DoState(PointerWrap& p)
{
}
