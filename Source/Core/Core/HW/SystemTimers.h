// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official Git repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

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
