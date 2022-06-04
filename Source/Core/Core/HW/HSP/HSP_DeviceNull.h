// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/HSP/HSP_Device.h"

namespace HSP
{
class CHSPDevice_Null : public IHSPDevice
{
public:
  explicit CHSPDevice_Null(HSPDeviceType device);

  void Write(u32 address, u64 value) override;
  u64 Read(u32 address) override;
};
}  // namespace HSP
