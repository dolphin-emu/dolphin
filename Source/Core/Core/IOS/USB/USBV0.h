// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"

// Used by early USB interfaces, such as /dev/usb/oh0 (except in IOS57, 58, 59) and /dev/usb/oh1.

namespace IOS::HLE
{
struct IOCtlVRequest;

namespace USB
{
enum V0Requests
{
  IOCTLV_USBV0_CTRLMSG = 0,
  IOCTLV_USBV0_BLKMSG = 1,
  IOCTLV_USBV0_INTRMSG = 2,
  IOCTL_USBV0_SUSPENDDEV = 5,
  IOCTL_USBV0_RESUMEDEV = 6,
  IOCTLV_USBV0_ISOMSG = 9,
  IOCTLV_USBV0_LBLKMSG = 10,
  IOCTLV_USBV0_GETDEVLIST = 12,
  IOCTL_USBV0_GETRHDESCA = 15,
  IOCTLV_USBV0_GETRHPORTSTATUS = 20,
  IOCTLV_USBV0_SETRHPORTSTATUS = 25,
  IOCTL_USBV0_DEVREMOVALHOOK = 26,
  IOCTLV_USBV0_DEVINSERTHOOK = 27,
  IOCTLV_USBV0_DEVICECLASSCHANGE = 28,
  IOCTL_USBV0_RESET_DEVICE = 29,
  IOCTLV_USBV0_DEVINSERTHOOKID = 30,
  IOCTL_USBV0_CANCEL_INSERT_HOOK = 31,
  IOCTLV_USBV0_UNKNOWN_32 = 32,
};

struct V0CtrlMessage final : CtrlMessage
{
  V0CtrlMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv);
};

struct V0BulkMessage final : BulkMessage
{
  V0BulkMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv, bool long_length = false);
};

struct V0IntrMessage final : IntrMessage
{
  V0IntrMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv);
};

struct V0IsoMessage final : IsoMessage
{
  V0IsoMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv);
};
}  // namespace USB
}  // namespace IOS::HLE
