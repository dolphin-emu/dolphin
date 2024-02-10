// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/SI/SI_DeviceGCController.h"

namespace SerialInterface
{
class CSIDevice_GCSteeringWheel final : public CSIDevice_GCController
{
public:
  CSIDevice_GCSteeringWheel(Core::System& system, SIDevices device, int device_number);

  int RunBuffer(u8* buffer, int request_length) override;
  bool GetData(u32& hi, u32& low) override;
  void SendCommand(u32 command, u8 poll) override;

private:
  enum class ForceCommandType : u8
  {
    MotorOn = 0x03,
    MotorOff = 0x02,
  };
};
}  // namespace SerialInterface
