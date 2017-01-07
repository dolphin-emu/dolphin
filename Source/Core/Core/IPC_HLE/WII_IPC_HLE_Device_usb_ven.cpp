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

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::IOCtlV(IOSResourceIOCtlVRequest& request)
{
  request.Dump(GetDeviceName());
  request.SetReturnValue(IPC_SUCCESS);
  return GetNoReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::IOCtl(IOSResourceIOCtlRequest& request)
{
  request.Log(GetDeviceName(), LogTypes::OSHLE);
  request.SetReturnValue(IPC_SUCCESS);

  IPCCommandResult reply = GetDefaultReply();
  switch (request.request)
  {
  case USBV5_IOCTL_GETVERSION:
    Memory::Write_U32(0x50001, request.out_addr);
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
    request.SetReturnValue(0);
    return reply;
  }
  break;

  case USBV5_IOCTL_ATTACHFINISH:
    reply = GetDefaultReply();
    break;

  case USBV5_IOCTL_SUSPEND_RESUME:
    DEBUG_LOG(OSHLE, "Device: %i Resumed: %i", Memory::Read_U32(request.in_addr),
              Memory::Read_U32(request.in_addr + 4));
    reply = GetDefaultReply();
    break;

  case USBV5_IOCTL_GETDEVPARAMS:
  {
    s32 device = Memory::Read_U32(request.in_addr);
    u32 unk = Memory::Read_U32(request.in_addr + 4);

    DEBUG_LOG(OSHLE, "USBV5_IOCTL_GETDEVPARAMS device: %i unk: %i", device, unk);

    Memory::Write_U32(0, request.out_addr);

    reply = GetDefaultReply();
  }
  break;

  default:
    DEBUG_LOG(OSHLE, "%x:%x %x:%x", request.in_addr, request.in_size, request.out_addr,
              request.out_size);
    break;
  }
  return reply;
}
