// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/USB_HID/HIDv5.h"

#include <string>

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
USB_HIDv5::USB_HIDv5(Kernel& ios, const std::string& device_name) : USBHost(ios, device_name)
{
}

USB_HIDv5::~USB_HIDv5()
{
  StopThreads();
}

IPCCommandResult USB_HIDv5::IOCtl(const IOCtlRequest& request)
{
  request.Log(GetDeviceName(), LogTypes::IOS_USB);
  switch (request.request)
  {
  case USB::IOCTL_USBV5_GETVERSION:
    Memory::Write_U32(VERSION, request.buffer_out);
    return GetDefaultReply(IPC_SUCCESS);
  case USB::IOCTL_USBV5_SHUTDOWN:
    if (m_hanging_request)
    {
      IOCtlRequest hanging_request{m_hanging_request};
      m_ios.EnqueueIPCReply(hanging_request, IPC_SUCCESS);
    }
    return GetDefaultReply(IPC_SUCCESS);
  case USB::IOCTL_USBV5_GETDEVICECHANGE:
    if (m_devicechange_replied)
    {
      m_hanging_request = request.address;
      return GetNoReply();
    }
    else
    {
      m_devicechange_replied = true;
      return GetDefaultReply(IPC_SUCCESS);
    }
  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_USB);
    return GetDefaultReply(IPC_SUCCESS);
  }
}

}  // namespace Device
}  // namespace HLE
}  // namespace IOS
