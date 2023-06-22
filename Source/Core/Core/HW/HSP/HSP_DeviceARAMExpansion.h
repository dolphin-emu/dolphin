// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/HSP/HSP_Device.h"

namespace HSP
{
class CHSPDevice_ARAMExpansion : public IHSPDevice
{
public:
  explicit CHSPDevice_ARAMExpansion(HSPDeviceType device);
  ~CHSPDevice_ARAMExpansion() override;

  void Write(u32 address, u64 value) override;
  u64 Read(u32 address) override;

  void DoState(PointerWrap&) override;

private:
  u32 m_size;
  u32 m_mask;
  u8* m_ptr = nullptr;
};
}  // namespace HSP
