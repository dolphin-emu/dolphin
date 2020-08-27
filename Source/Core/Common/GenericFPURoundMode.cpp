// Copyright 2003 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"

// Generic, do nothing
namespace FPURoundMode
{
void SetRoundMode(int mode)
{
}
void SetPrecisionMode(PrecisionMode mode)
{
}
void SetSIMDMode(int rounding_mode, bool non_ieee_mode)
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
