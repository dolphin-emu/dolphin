// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace DVDMath
{
double CalculatePhysicalDiscPosition(u64 offset);
double CalculateSeekTime(u64 offset_from, u64 offset_to);
double CalculateRotationalLatency(u64 offset, double time, bool wii_disc);
double CalculateRawDiscReadTime(u64 offset, u64 length, bool wii_disc);

}  // namespace DVDMath
