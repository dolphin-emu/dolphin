// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

// TODO: Get rid of this file.

namespace DSP
{
#define PROFILE 0

#if PROFILE
void ProfilerDump(u64 _count);
void ProfilerInit();
void ProfilerAddDelta(int _addr, int _delta);
void ProfilerStart();
#endif
}  // namespace DSP
