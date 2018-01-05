// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"

// Used by late USB interfaces for /dev/usb/ven and /dev/usb/hid (since IOS57 which
// reorganised the USB modules in IOS).

namespace IOS
{
namespace HLE
{
struct IOCtlRequest;

namespace USB
{
enum V5Requests
{
  IOCTL_USBV5_GETVERSION = 0,
  IOCTL_USBV5_GETDEVICECHANGE = 1,
  IOCTL_USBV5_SHUTDOWN = 2,
  IOCTL_USBV5_GETDEVPARAMS = 3,
  IOCTL_USBV5_ATTACHFINISH = 6,
  IOCTL_USBV5_SETALTERNATE = 7,
  IOCTL_USBV5_SUSPEND_RESUME = 16,
  IOCTL_USBV5_CANCELENDPOINT = 17,
  IOCTLV_USBV5_CTRLMSG = 18,
  IOCTLV_USBV5_INTRMSG = 19,
  IOCTLV_USBV5_ISOMSG = 20,
  IOCTLV_USBV5_BULKMSG = 21
};

struct V5CtrlMessage final : CtrlMessage
{
  V5CtrlMessage(Kernel& ios, const IOCtlVRequest& ioctlv);
};

struct V5BulkMessage final : BulkMessage
{
  V5BulkMessage(Kernel& ios, const IOCtlVRequest& ioctlv);
};

struct V5IntrMessage final : IntrMessage
{
  V5IntrMessage(Kernel& ios, const IOCtlVRequest& ioctlv);
};

struct V5IsoMessage final : IsoMessage
{
  V5IsoMessage(Kernel& ios, const IOCtlVRequest& cmd_buffer);
};
}  // namespace USB
}  // namespace HLE
}  // namespace IOS
