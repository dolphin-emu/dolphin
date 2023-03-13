// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SI/SI_DeviceGCController.h"
#include "InputCommon/GCPadStatus.h"

namespace SerialInterface
{
class CSIDevice_GCAdapter final : public CSIDevice_GCController
{
public:
  CSIDevice_GCAdapter(Core::System& system, SIDevices device, int device_number);

  GCPadStatus GetPadStatus() override;
  int RunBuffer(u8* buffer, int request_length) override;

  bool GetData(u32& hi, u32& low) override;

private:
  bool m_simulate_konga{};
};
}  // namespace SerialInterface
