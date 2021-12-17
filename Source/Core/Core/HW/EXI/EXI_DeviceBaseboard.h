// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace ExpansionInterface
{
class CEXIBaseboard : public IEXIDevice
{
public:
  CEXIBaseboard();
  void SetCS(int cs) override;
  bool IsPresent() const override;
  bool IsInterruptSet();
  void DoState(PointerWrap& p) override;

private:
  // STATE_TO_SAVE
  bool m_have_irq;
  u32 m_position = 0;
  u32 m_command = 0;
  u8* m_subcommand = (u8*)&m_command;
  u32 m_irq_timer;
  u32 m_irq_status;

  void TransferByte(u8& byte) override;
};
}  // namespace ExpansionInterface
