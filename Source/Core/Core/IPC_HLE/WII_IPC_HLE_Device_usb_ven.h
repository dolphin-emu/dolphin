// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class CWII_IPC_HLE_Device_usb_ven : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_usb_ven(u32 DeviceID, const std::string& DeviceName);

  virtual ~CWII_IPC_HLE_Device_usb_ven();

  virtual IPCCommandResult Open(u32 CommandAddress, u32 Mode);
  virtual IPCCommandResult Close(u32 CommandAddress, bool Force);

  virtual IPCCommandResult IOCtlV(u32 CommandAddress);
  virtual IPCCommandResult IOCtl(u32 CommandAddress);

  virtual u32 Update();

  void DoState(PointerWrap &p);

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
