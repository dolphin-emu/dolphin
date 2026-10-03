// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/HSP/HSP_Device.h"

namespace HSP
{
class CHSPDevice_ARAMExpansion final : public IHSPDevice
{
public:
  explicit CHSPDevice_ARAMExpansion();
  ~CHSPDevice_ARAMExpansion() override;

  HSPDeviceType GetDeviceType() const override { return HSPDeviceType::ARAMExpansion; }

  void Read(u32 address, std::span<u8, TRANSFER_SIZE> data) override;
  void Write(u32 address, std::span<const u8, TRANSFER_SIZE> data) override;

  void DoState(PointerWrap&) override;

private:
  const u32 m_size;
  const u32 m_mask;
  u8* const m_ptr = nullptr;
};
}  // namespace HSP
