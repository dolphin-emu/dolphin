// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/IPC_HLE/USB/Common.h"

struct SIOCtlVBuffer;

// Used by early USB interfaces, such as /dev/usb/oh0 and /dev/usb/oh1.

enum USBV0IOCtl
{
  USBV0_IOCTL_CTRLMSG = 0,
  USBV0_IOCTL_BLKMSG = 1,
  USBV0_IOCTL_INTRMSG = 2,
  USBV0_IOCTL_SUSPENDDEV = 5,
  USBV0_IOCTL_RESUMEDEV = 6,
  USBV0_IOCTL_ISOMSG = 9,
  USBV0_IOCTL_GETDEVLIST = 12,
  USBV0_IOCTL_DEVREMOVALHOOK = 26,
  USBV0_IOCTL_DEVINSERTHOOK = 27,
  USBV0_IOCTL_DEVICECLASSCHANGE = 28,
  // Unknown IOCtl used by Wheel of Fortune on game shutdown.
  USBV0_IOCTL_UNKNOWN_29 = 29,
  // Unknown IOCtl.
  USBV0_IOCTL_UNKNOWN_30 = 30,
};

struct USBV0CtrlMessage : CtrlMessage
{
  USBV0CtrlMessage(const SIOCtlVBuffer& cmd_buffer);
};

struct USBV0BulkMessage : BulkMessage
{
  USBV0BulkMessage() = default;
  USBV0BulkMessage(const SIOCtlVBuffer& cmd_buffer);
};

struct USBV0IntrMessage : IntrMessage
{
  USBV0IntrMessage() = default;
  USBV0IntrMessage(const SIOCtlVBuffer& cmd_buffer);
};

struct USBV0IsoMessage : IsoMessage
{
  USBV0IsoMessage(const SIOCtlVBuffer& cmd_buffer);
};
