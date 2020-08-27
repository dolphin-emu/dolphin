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
struct Globals
{
  s64 global_timer;
  int slice_length;
  u64 fake_TB_start_value;
  u64 fake_TB_start_ticks;
  float last_OC_factor_inverted;
};
extern Globals g;

// CoreTiming begins at the boundary of timing slice -1. An initial call to Advance() is
// required to end slice -1 and start slice 0 before the first cycle of code is executed.
void Init();
void Shutdown();

typedef void (*TimedCallback)(u64 userdata, s64 cyclesLate);

// This should only be called from the CPU thread, if you are calling it any other thread, you are
// doing something evil
u64 GetTicks();
u64 GetIdleTicks();

void DoState(PointerWrap& p);

struct EventType;

// Returns the event_type identifier. if name is not unique, an existing event_type will be
// discarded.
EventType* RegisterEvent(const std::string& name, TimedCallback callback);
void UnregisterAllEvents();

enum class FromThread
{
  CPU,
  NON_CPU,
  // Don't use ANY unless you're sure you need to call from
  // both the CPU thread and at least one other thread
  ANY
};

// userdata MAY NOT CONTAIN POINTERS. userdata might get written and reloaded from savestates.
// After the first Advance, the slice lengths and the downcount will be reduced whenever an event
// is scheduled earlier than the current values (when scheduled from the CPU Thread only).
// Scheduling from a callback will not update the downcount until the Advance() completes.
void ScheduleEvent(s64 cycles_into_future, EventType* event_type, u64 userdata = 0,
                   FromThread from = FromThread::CPU);

// We only permit one event of each type in the queue at a time.
void RemoveEvent(EventType* event_type);
void RemoveAllEvents(EventType* event_type);

// Advance must be called at the beginning of dispatcher loops, not the end. Advance() ends
// the previous timing slice and begins the next one, you must Advance from the previous
// slice to the current one before executing any cycles. CoreTiming starts in slice -1 so an
// Advance() is required to initialize the slice length before the first cycle of emulated
// instructions is executed.
// NOTE: Advance updates the PowerPC downcount and performs a PPC external exception check.
void Advance();
void MoveEvents();

// Pretend that the main CPU has executed enough cycles to reach the next event.
void Idle();

// Clear all pending events. This should ONLY be done on exit or state load.
void ClearPendingEvents();

void LogPendingEvents();

std::string GetScheduledEventsSummary();

void AdjustEventQueueTimes(u32 new_ppc_clock, u32 old_ppc_clock);

u32 GetFakeDecStartValue();
void SetFakeDecStartValue(u32 val);
u64 GetFakeDecStartTicks();
void SetFakeDecStartTicks(u64 val);
u64 GetFakeTBStartValue();
void SetFakeTBStartValue(u64 val);
u64 GetFakeTBStartTicks();
void SetFakeTBStartTicks(u64 val);

void ForceExceptionCheck(s64 cycles);

}  // namespace CoreTiming
