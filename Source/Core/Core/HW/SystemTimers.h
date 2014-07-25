// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace SystemTimers
{

u32 GetTicksPerSecond();
void PreInit();
void Init();
void Shutdown();

// Notify timing system that somebody wrote to the decrementer
void DecrementerSet();
u32 GetFakeDecrementer();

void TimeBaseSet();
u64 GetFakeTimeBase();

}
