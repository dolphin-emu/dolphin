// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace SystemTimers
{

/*
GameCube                   MHz
flipper <-> ARAM bus:      81 (DSP)
gekko <-> flipper bus:     162
flipper <-> 1T-SRAM bus:   324
gekko:                     486

These contain some guesses:
Wii                             MHz
hollywood <-> GDDR3 RAM bus:    ??? no idea really
broadway <-> hollywood bus:     243
hollywood <-> 1T-SRAM bus:      486
broadway:                       729
*/
// Ratio of TB and Decrementer to clock cycles.
// TB clk is 1/4 of BUS clk. And it seems BUS clk is really 1/3 of CPU clk.
// So, ratio is 1 / (1/4 * 1/3 = 1/12) = 12.
// note: ZWW is ok and faster with TIMER_RATIO=8 though.
// !!! POSSIBLE STABLE PERF BOOST HACK THERE !!!

enum
{
	TIMER_RATIO = 12
};

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
