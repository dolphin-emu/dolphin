// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// This is a system to schedule events into the emulated machine's future. Time is measured
// in main CPU clock cycles.

// To schedule an event, you first have to register its type. This is where you pass in the
// callback. You then schedule events using the type id you get back.

// See HW/SystemTimers.cpp for the main part of Dolphin's usage of this scheduler.

// The int cyclesLate that the callbacks get is how many cycles late it was.
// So to schedule a new event on a regular basis:
// inside callback:
//   ScheduleEvent(periodInCycles - cyclesLate, callback, "whatever")

#include <string>
#include "Common/CommonTypes.h"

class PointerWrap;

namespace CoreTiming
{

// These really shouldn't be global, but jit64 accesses them directly
extern s64 g_globalTimer;
extern u64 g_fakeTBStartValue;
extern u64 g_fakeTBStartTicks;
extern int g_slicelength;
extern float g_lastOCFactor_inverted;

void Init();
void Shutdown();

typedef void (*TimedCallback)(u64 userdata, s64 cyclesLate);

// This should only be called from the CPU thread, if you are calling it any other thread, you are doing something evil
u64 GetTicks();
u64 GetIdleTicks();

void DoState(PointerWrap &p);

// Returns the event_type identifier. if name is not unique, an existing event_type will be discarded.
int RegisterEvent(const std::string& name, TimedCallback callback);
void UnregisterAllEvents();

// userdata MAY NOT CONTAIN POINTERS. userdata might get written and reloaded from savestates.
void ScheduleEvent(s64 cyclesIntoFuture, int event_type, u64 userdata = 0);
void ScheduleEvent_Immediate(int event_type, u64 userdata = 0);
void ScheduleEvent_Threadsafe(s64 cyclesIntoFuture, int event_type, u64 userdata = 0);
void ScheduleEvent_Threadsafe_Immediate(int event_type, u64 userdata = 0);
void ScheduleEvent_AnyThread(s64 cyclesIntoFuture, int event_type, u64 userdata = 0);

// We only permit one event of each type in the queue at a time.
void RemoveEvent(int event_type);
void RemoveAllEvents(int event_type);
void Advance();
void MoveEvents();
void ProcessFifoWaitEvents();

// Pretend that the main CPU has executed enough cycles to reach the next event.
void Idle();

// Clear all pending events. This should ONLY be done on exit or state load.
void ClearPendingEvents();

void LogPendingEvents();

std::string GetScheduledEventsSummary();

u32 GetFakeDecStartValue();
void SetFakeDecStartValue(u32 val);
u64 GetFakeDecStartTicks();
void SetFakeDecStartTicks(u64 val);
u64 GetFakeTBStartValue();
void SetFakeTBStartValue(u64 val);
u64 GetFakeTBStartTicks();
void SetFakeTBStartTicks(u64 val);

void ForceExceptionCheck(s64 cycles);



} // end of namespace
