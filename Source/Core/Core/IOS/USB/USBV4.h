// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"

// Used by an early version of /dev/usb/hid.

namespace IOS
{
namespace HLE
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
  explicit V4CtrlMessage(const IOCtlRequest& ioctl);
};

struct V4GetUSStringMessage final : CtrlMessage
{
  explicit V4GetUSStringMessage(const IOCtlRequest& ioctl);
  void OnTransferComplete() const override;
};

struct V4IntrMessage final : IntrMessage
{
  explicit V4IntrMessage(const IOCtlRequest& ioctl);
};
}  // namespace USB
}  // namespace HLE
}  // namespace IOS
