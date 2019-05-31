// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/HSP/HSP_Device.h"

namespace HSP
{
class CHSPDevice_Null : public IHSPDevice
{
public:
  explicit CHSPDevice_Null(HSPDevices device);

  void Write(u32 address, u64 value) override;
  u64 Read(u32 address) override;
};
}  // namespace HSP
