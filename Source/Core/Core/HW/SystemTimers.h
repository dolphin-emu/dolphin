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

enum class Mode
{
  GC,
  Wii,
};

u32 GetTicksPerSecond();
void PreInit();
void Init();
void Shutdown();
void ChangePPCClock(Mode mode);

// Notify timing system that somebody wrote to the decrementer
void DecrementerSet();
u32 GetFakeDecrementer();

void TimeBaseSet();
u64 GetFakeTimeBase();
// Custom RTC
s64 GetLocalTimeRTCOffset();

// Returns an estimate of how fast/slow the emulation is running (excluding throttling induced sleep
// time). The estimate is computed over the last 1s of emulated time. Example values:
//
// - 0.5: the emulator is running at 50% speed (falling behind).
// - 1.0: the emulator is running at 100% speed.
// - 2.0: the emulator is running at 200% speed (or 100% speed but sleeping half of the time).
double GetEstimatedEmulationPerformance();

}  // namespace SystemTimers
