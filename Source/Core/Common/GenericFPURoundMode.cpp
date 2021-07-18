// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"

// Generic, do nothing
namespace FPURoundMode
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
}  // namespace FPURoundMode
