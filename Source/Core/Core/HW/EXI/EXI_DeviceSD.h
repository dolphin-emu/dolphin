// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI_Device.h"

namespace ExpansionInterface
{
// EXI-SD adapter (DOL-019)
class CEXISD final : public IEXIDevice
{
public:
  explicit CEXISD(Core::System& system);

  void ImmWrite(u32 data, u32 size) override;
  u32 ImmRead(u32 size) override;

  void DMAWrite(u32 address, u32 size) override;
  void DMARead(u32 address, u32 size) override;

  bool IsPresent() const override;

private:
  void TransferByte(u8& byte) override;
};
}  // namespace ExpansionInterface
