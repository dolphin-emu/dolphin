// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI/SI_DeviceGCController.h"

namespace SerialInterface
{
class CSIDevice_GCSteeringWheel : public CSIDevice_GCController
{
public:
  CSIDevice_GCSteeringWheel(SIDevices device, int device_number);

  int RunBuffer(u8* buffer, int length) override;
  bool GetData(u32& hi, u32& low) override;
  void SendCommand(u32 command, u8 poll) override;

private:
  // Commands
  enum EBufferCommands
  {
    CMD_RESET = 0x00,
    CMD_ORIGIN = 0x41,
    CMD_RECALIBRATE = 0x42,
    CMD_ID = 0xff,
  };

  enum EDirectCommands
  {
    CMD_FORCE = 0x30,
    CMD_WRITE = 0x40
  };
};
}  // namespace SerialInterface
