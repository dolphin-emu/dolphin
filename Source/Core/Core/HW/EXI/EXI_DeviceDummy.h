// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI_Device.h"

namespace ExpansionInterface
{
// Just a dummy that logs reads and writes
// to be used for EXI devices we haven't emulated
// DOES NOT FUNCTION AS "NO DEVICE INSERTED" -> Appears as unknown device
class CEXIDummy final : public IEXIDevice
{
public:
  CEXIDummy(Core::System& system, const std::string& name);

  void ImmWrite(u32 data, u32 size) override;
  u32 ImmRead(u32 size) override;
  void ImmReadWrite(u32& data, u32 size) override;

  void DMAWrite(u32 address, u32 size) override;
  void DMARead(u32 address, u32 size) override;

  bool IsPresent() const override;

private:
  void TransferByte(u8& byte) override;

  std::string m_name;
};
}  // namespace ExpansionInterface
