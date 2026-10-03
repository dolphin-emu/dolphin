// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on: https://github.com/GXTX/YACardEmu
// Copyright (C) 2020-2023 wutno (https://github.com/GXTX)
// Copyright (C) 2022-2023 tugpoat (https://github.com/tugpoat)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/MagCard/MagneticCardReader.h"

namespace MagCard
{

// CRP-1231BR-10
// Used by F-Zero AX
class C1231BR final : public MagneticCardReader
{
public:
  using MagneticCardReader::MagneticCardReader;

protected:
  // Specific to this model.
  bool m_is_shutter_open = true;

  bool IsReadyForCard() override;

  u8 GetPositionValue() override;

  void Command_10_Initialize() override;
  void Command_D0_ControlShutter() override;

  void DoState(PointerWrap& p) override;
};

}  // namespace MagCard
