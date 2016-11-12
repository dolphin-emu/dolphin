// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/USB/Common.h"

struct IOCtlBuffer;

// Used by an early version of /dev/usb/hid.

enum USBV4IOCtl
{
  USBV4_IOCTL_GETDEVICECHANGE = 0,
  USBV4_IOCTL_SET_SUSPEND = 1,
  USBV4_IOCTL_CTRLMSG = 2,
  USBV4_IOCTL_INTRMSG_IN = 3,
  USBV4_IOCTL_INTRMSG_OUT = 4,
  USBV4_IOCTL_GET_US_STRING = 5,
  USBV4_IOCTL_GETVERSION = 6,
  USBV4_IOCTL_SHUTDOWN = 7,
  USBV4_IOCTL_CANCELINTERRUPT = 8,
};

// Used when only the device ID is needed.
struct USBV4Request final
{
  explicit USBV4Request(const IOCtlBuffer& cmd_buffer);
  u32 cmd_address;
  s32 device_id;
};

struct USBV4CtrlMessage final : CtrlMessage
{
  explicit USBV4CtrlMessage(const IOCtlBuffer& cmd_buffer);
};

struct USBV4GetUSStringMessage final : CtrlMessage
{
  explicit USBV4GetUSStringMessage(const IOCtlBuffer& cmd_buffer);
};

struct USBV4IntrMessage final : IntrMessage
{
  explicit USBV4IntrMessage(const IOCtlBuffer& cmd_buffer);
};
