// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IPC.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
class USB_VEN final : public Device
{
public:
  USB_VEN(u32 device_id, const std::string& device_name);

  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

private:
  enum USBIOCtl
  {
    USBV5_IOCTL_GETVERSION = 0,
    USBV5_IOCTL_GETDEVICECHANGE = 1,
    USBV5_IOCTL_SHUTDOWN = 2,
    USBV5_IOCTL_GETDEVPARAMS = 3,
    USBV5_IOCTL_ATTACHFINISH = 6,
    USBV5_IOCTL_SETALTERNATE = 7,
    USBV5_IOCTL_SUSPEND_RESUME = 16,
    USBV5_IOCTL_CANCELENDPOINT = 17,
    USBV5_IOCTL_CTRLMSG = 18,
    USBV5_IOCTL_INTRMSG = 19,
    USBV5_IOCTL_ISOMSG = 20,
    USBV5_IOCTL_BULKMSG = 21
  };
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
