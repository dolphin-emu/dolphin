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

// This is a system to schedule events into the emulated machine's future. Time is measured
// in main CPU clock cycles.

// To schedule an event, you first have to register its type. This is where you pass in the
// callback. You then schedule events using the type id you get back.

// See HW/SystemTimers.cpp for the main part of Dolphin's usage of this scheduler.

// The int cyclesLate that the callbacks get is how many cycles late it was.
// So to schedule a new event on a regular basis:
// inside callback:
//   ScheduleEvent(periodInCycles - cyclesLate, callback, "whatever")

#include "Common.h"

#include <string>

#include "ChunkFile.h"

namespace CoreTiming
{

void Init();
void Shutdown();

typedef void (*TimedCallback)(u64 userdata, int cyclesLate);

u64 GetTicks();
u64 GetIdleTicks();

void DoState(PointerWrap &p);

// Returns the event_type identifier.
int RegisterEvent(const char *name, TimedCallback callback);
void UnregisterAllEvents();

// userdata MAY NOT CONTAIN POINTERS. userdata might get written and reloaded from disk,
// when we implement state saves.
void ScheduleEvent(int cyclesIntoFuture, int event_type, u64 userdata=0);
void ScheduleEvent_Threadsafe(int cyclesIntoFuture, int event_type, u64 userdata=0);

// We only permit one event of each type in the queue at a time.
void RemoveEvent(int event_type);
bool IsScheduled(int event_type);
void Advance();

// Pretend that the main CPU has executed enough cycles to reach the next event.
void Idle();

// Clear all pending events. This should ONLY be done on exit or state load.
void ClearPendingEvents();

void LogPendingEvents();
void SetMaximumSlice(int maximumSliceLength);

void RegisterAdvanceCallback(void (*callback)(int cyclesExecuted));

std::string GetScheduledEventsSummary();

extern int downcount;
extern int slicelength;

}; // end of namespace

#endif
