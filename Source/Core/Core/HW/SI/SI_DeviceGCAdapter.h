// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SI/SI_DeviceGCController.h"
#include "InputCommon/GCPadStatus.h"

namespace SerialInterface
{
class CSIDevice_GCAdapter : public CSIDevice_GCController
{
public:
  CSIDevice_GCAdapter(SIDevices device, int device_number);

  GCPadStatus GetPadStatus() override;
  int RunBuffer(u8* buffer, int length) override;

  bool GetData(u32& hi, u32& low) override;

private:
  bool m_simulate_konga{};
};
}  // namespace SerialInterface
