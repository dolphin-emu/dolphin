// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "Core/DSP/DSPInterpreter.h"

// Basic code analysis.
namespace DSPAnalyzer
{
#define ISPACE 65536

// Useful things to detect:
// * Loop endpoints - so that we can avoid checking for loops every cycle.

enum
{
	CODE_START_OF_INST = 1,
	CODE_IDLE_SKIP     = 2,
	CODE_LOOP_START    = 4,
	CODE_LOOP_END      = 8,
	CODE_UPDATE_SR     = 16,
	CODE_CHECK_INT     = 32,
};

// Easy to query array covering the whole of instruction memory.
// Just index by address.
// This one will be helpful for debuggers and jits.
extern std::array<u8, ISPACE> code_flags;

// This one should be called every time IRAM changes - which is basically
// every time that a new ucode gets uploaded, and never else. At that point,
// we can do as much static analysis as we want - but we should always throw
// all old analysis away. Luckily the entire address space is only 64K code
// words and the actual code space 8K instructions in total, so we can do
// some pretty expensive analysis if necessary.
void Analyze();

}  // namespace
