// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on: https://github.com/GXTX/YACardEmu
// Copyright (C) 2020-2023 wutno (https://github.com/GXTX)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/MagCard/MagneticCardReader.h"

namespace MagCard
{

// CRP-1231LR-10NAB
// Used by Mario Kart Arcade GP + GP2
class C1231LR final : public MagneticCardReader
{
public:
  using MagneticCardReader::MagneticCardReader;

protected:
  u8 GetPositionValue() override;
};

}  // namespace MagCard
