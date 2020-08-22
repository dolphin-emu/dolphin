// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace ExpansionInterface
{
// EXI-SD adapter (DOL-019)
class CEXISD final : public IEXIDevice
{
public:
  explicit CEXISD(Core::System& system);

  void ImmWrite(u32 data, u32 size) override;
  u32 ImmRead(u32 size) override;
  void SetCS(int cs) override;

  bool IsPresent() const override;
  void DoState(PointerWrap& p) override;

private:
  void TransferByte(u8& byte) override;

  // STATE_TO_SAVE
  bool inited = false;
  bool get_id = false;
  int command = 0;
  u32 m_uPosition = 0;
};
}  // namespace ExpansionInterface
