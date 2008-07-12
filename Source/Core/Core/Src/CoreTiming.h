// Copyright (C) 2003-2008 Dolphin Project.

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
#ifndef _CORETIMING_H
#define _CORETIMING_H

#include "Common.h"

namespace CoreTiming
{

enum {
	SOON = 100
};

typedef void (*TimedCallback)(u64 userdata, int cyclesLate);

u64 GetTicks();
u64 GetIdleTicks();
// The int that the callbacks get is how many cycles late it was.
// So to schedule a new event on a regular basis:
// inside callback:
//   ScheduleEvent(periodInCycles - cyclesLate, callback, "whatever")
void ScheduleEvent(int cyclesIntoFuture, TimedCallback callback, const char *name, u64 userdata=0);
void ScheduleEvent_Threadsafe(int cyclesIntoFuture, TimedCallback callback, const char *name, u64 userdata=0);
void RemoveEvent(TimedCallback callback);
bool IsScheduled(TimedCallback callback);
void Advance();
void Idle();
void Clear();
void LogPendingEvents();
void SetMaximumSlice(int maximumSliceLength);

void RegisterAdvanceCallback(void (*callback)(int cyclesExecuted));

extern int downcount;
extern int slicelength;

}; // end of namespace

#endif
