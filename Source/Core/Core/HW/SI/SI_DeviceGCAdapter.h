// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

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
  int RunBuffer(u8* buffer, int request_length) override;

  void OnSimulatedDeviceChanged(const SIDevices device_type, const int device_number);

private:
  std::unique_ptr<ISIDevice> m_simulated_device;
};
}  // namespace SerialInterface
