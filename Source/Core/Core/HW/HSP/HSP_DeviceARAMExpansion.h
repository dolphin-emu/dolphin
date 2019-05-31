// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/HSP/HSP_Device.h"

namespace HSP
{
class CHSPDevice_ARAMExpansion : public IHSPDevice
{
public:
  explicit CHSPDevice_ARAMExpansion(HSPDevices device);
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
