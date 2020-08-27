// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

// Basic code analysis.
namespace DSP::Analyzer
{
// Useful things to detect:
// * Loop endpoints - so that we can avoid checking for loops every cycle.

enum
{
  CODE_START_OF_INST = 1,
  CODE_IDLE_SKIP = 2,
  CODE_LOOP_START = 4,
  CODE_LOOP_END = 8,
  CODE_UPDATE_SR = 16,
  CODE_CHECK_INT = 32,
};

// This one should be called every time IRAM changes - which is basically
// every time that a new ucode gets uploaded, and never else. At that point,
// we can do as much static analysis as we want - but we should always throw
// all old analysis away. Luckily the entire address space is only 64K code
// words and the actual code space 8K instructions in total, so we can do
// some pretty expensive analysis if necessary.
void Analyze();

// Retrieves the flags set during analysis for code in memory.
u8 GetCodeFlags(u16 address);

}  // namespace DSP::Analyzer
