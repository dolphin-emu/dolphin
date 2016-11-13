// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/IPC_HLE/USB/Common.h"

struct SIOCtlVBuffer;

// Used by late USB interfaces for /dev/usb/ven and /dev/usb/hid (since IOS57 which
// reorganised the USB modules in IOS).

enum USBV5IOCtl
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

// Used when only the device ID is needed.
struct USBV5TransferCommand final : TransferCommand
{
  explicit USBV5TransferCommand(const SIOCtlVBuffer& cmd_buffer);
};

struct USBV5CtrlMessage final : CtrlMessage
{
  explicit USBV5CtrlMessage(const SIOCtlVBuffer& cmd_buffer);
};

struct USBV5BulkMessage final : BulkMessage
{
  explicit USBV5BulkMessage(const SIOCtlVBuffer& cmd_buffer);
};

struct USBV5IntrMessage final : IntrMessage
{
  explicit USBV5IntrMessage(const SIOCtlVBuffer& cmd_buffer);
};

struct USBV5IsoMessage final : IsoMessage
{
  explicit USBV5IsoMessage(const SIOCtlVBuffer& cmd_buffer);
};
