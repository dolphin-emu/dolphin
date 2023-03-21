// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FPURoundMode.h"

#include "Common/CommonTypes.h"

// Generic, do nothing
namespace Common::FPU
{
void SetSIMDMode(RoundMode rounding_mode, bool non_ieee_mode)
{
}
void SaveSIMDState()
{
}
void LoadSIMDState()
{
}
void LoadDefaultSIMDState()
{
}
}  // namespace Common::FPU
