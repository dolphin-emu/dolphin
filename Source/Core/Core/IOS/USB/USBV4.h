// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"

// Used by an early version of /dev/usb/hid.

namespace IOS::HLE
{
struct IOCtlRequest;

namespace USB
{
enum V4Requests
{
  IOCTL_USBV4_GETDEVICECHANGE = 0,
  IOCTL_USBV4_SET_SUSPEND = 1,
  IOCTL_USBV4_CTRLMSG = 2,
  IOCTL_USBV4_INTRMSG_IN = 3,
  IOCTL_USBV4_INTRMSG_OUT = 4,
  IOCTL_USBV4_GET_US_STRING = 5,
  IOCTL_USBV4_GETVERSION = 6,
  IOCTL_USBV4_SHUTDOWN = 7,
  IOCTL_USBV4_CANCELINTERRUPT = 8,
};

struct V4CtrlMessage final : CtrlMessage
{
  V4CtrlMessage(EmulationKernel& ios, const IOCtlRequest& ioctl);
};

struct V4GetUSStringMessage final : CtrlMessage
{
  V4GetUSStringMessage(EmulationKernel& ios, const IOCtlRequest& ioctl);
  void OnTransferComplete(s32 return_value) const override;
};

struct V4IntrMessage final : IntrMessage
{
  V4IntrMessage(EmulationKernel& ios, const IOCtlRequest& ioctl);
};
}  // namespace USB
}  // namespace IOS::HLE
