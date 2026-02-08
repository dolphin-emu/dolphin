// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/SI/SI_Device.h"

namespace SerialInterface
{
// Stub class for saying nothing is attached, and not having to deal with null pointers :)
class CSIDevice_Null final : public ISIDevice
{
public:
  CSIDevice_Null(Core::System& system, SIDevices device, int device_number);

  int RunBuffer(u8* buffer, int request_length) override;
  bool GetData(u32& hi, u32& low) override;
  void SendCommand(u32 command, u8 poll) override;
};
}  // namespace SerialInterface
