// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/SI/SI_DeviceGCController.h"

struct GCPadStatus;

namespace SerialInterface
{
class CSIDevice_DanceMat : public CSIDevice_GCController
{
public:
  CSIDevice_DanceMat(SIDevices device, int device_number);

  u32 MapPadStatus(const GCPadStatus& pad_status) override;
  bool GetData(u32& hi, u32& low) override;
  TSIDevices GetControllerId() const override;
};
}  // namespace SerialInterface
