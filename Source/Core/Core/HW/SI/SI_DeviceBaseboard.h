// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/SI/SI_Device.h"

namespace SerialInterface
{
class CSIDevice_Baseboard : public ISIDevice
{
public:
  // Constructor
  CSIDevice_Baseboard(SIDevices device, int device_number);

  bool GetData(u32& hi, u32& low) override;
  void SendCommand(u32 command, u8 poll) override;
};
}  // namespace SerialInterface
