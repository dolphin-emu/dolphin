// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// Basic code analysis.

#include "DSPInterpreter.h"

namespace DSPAnalyzer {

#define ISPACE 65536



// Useful things to detect:
// * Loop endpoints - so that we can avoid checking for loops every cycle.

enum
{
	CODE_START_OF_INST = 1,
	CODE_IDLE_SKIP = 2,
	CODE_LOOP_END = 4,
};

// Easy to query array covering the whole of instruction memory.
// Just index by address.
// This one will be helpful for debuggers and jits.
extern u8 code_flags[ISPACE];

// This one should be called every time IRAM changes - which is basically
// every time that a new ucode gets uploaded, and never else. At that point,
// we can do as much static analysis as we want - but we should always throw
// all old analysis away. Luckily the entire address space is only 64K code
// words and the actual code space 8K instructions in total, so we can do
// some pretty expensive analysis if necessary.
void Analyze();

}  // namespace