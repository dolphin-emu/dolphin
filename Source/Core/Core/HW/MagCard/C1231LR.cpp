// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on: https://github.com/GXTX/YACardEmu
// Copyright (C) 2020-2023 wutno (https://github.com/GXTX)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/MagCard/C1231LR.h"

namespace MagCard
{

u8 C1231LR::GetPositionValue()
{
  // True "R" (Card Position) values.
  static constexpr std::array<u8, std::size_t(R::MAX_POSITIONS)> POSITION_VALUES{
      0x30,  // NO_CARD
      0x31,  // READ_WRITE_HEAD
      0x32,  // THERMAL_HEAD
      0x33,  // DISPENSER_THERMAL
      0x34,  // ENTRY_SLOT
  };

  return POSITION_VALUES[u8(m_status.r)];
}

}  // namespace MagCard
