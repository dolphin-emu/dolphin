// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/SPSCQueue.h"
#include "Core/CPUThreadConfigCallback.h"

class PointerWrap;

namespace Core
{
class System;
}

namespace CoreTiming
{
// These really shouldn't be global, but jit64 accesses them directly
struct Globals
{
  s64 global_timer = 0;
  int slice_length = 0;
  u64 fake_TB_start_value = 0;
  u64 fake_TB_start_ticks = 0;
  float last_OC_factor_inverted = 0.0f;
};

typedef void (*TimedCallback)(Core::System& system, u64 userdata, s64 cyclesLate);

struct EventType
{
  TimedCallback callback;
  const std::string* name;
};

struct Event
{
  s64 time;
  u64 fifo_order;
  u64 userdata;
  EventType* type;
};

enum class FromThread
{
  CPU,
  NON_CPU,
  // Don't use ANY unless you're sure you need to call from
  // both the CPU thread and at least one other thread
  ANY
};

// helpers until the JIT is updated to use the instance
void GlobalAdvance();
void GlobalIdle();

class CoreTimingManager
{
public:
  explicit CoreTimingManager(Core::System& system);

  // CoreTiming begins at the boundary of timing slice -1. An initial call to Advance() is
  // required to end slice -1 and start slice 0 before the first cycle of code is executed.
  void Init();
  void Shutdown();

  // This should only be called from the CPU thread, if you are calling it any other thread, you are
  // doing something evil
  u64 GetTicks() const;
  u64 GetIdleTicks() const;

  void RefreshConfig();

  void DoState(PointerWrap& p);

  // Returns the event_type identifier. if name is not unique, an existing event_type will be
  // discarded.
  EventType* RegisterEvent(const std::string& name, TimedCallback callback);
  void UnregisterAllEvents();

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

  void LogPendingEvents() const;

  std::string GetScheduledEventsSummary() const;

  void AdjustEventQueueTimes(u32 new_ppc_clock, u32 old_ppc_clock);

  u32 GetFakeDecStartValue() const;
  void SetFakeDecStartValue(u32 val);
  u64 GetFakeDecStartTicks() const;
  void SetFakeDecStartTicks(u64 val);
  u64 GetFakeTBStartValue() const;
  void SetFakeTBStartValue(u64 val);
  u64 GetFakeTBStartTicks() const;
  void SetFakeTBStartTicks(u64 val);

  void ForceExceptionCheck(s64 cycles);

  // Directly accessed by the JIT.
  Globals& GetGlobals() { return m_globals; }

  // Throttle the CPU to the specified target cycle.
  // Never used outside of CoreTiming, however it remains public
  // in order to allow custom throttling implementations to be tested.
  void Throttle(const s64 target_cycle);

  TimePoint GetCPUTimePoint(s64 cyclesLate) const;  // Used by Dolphin Analytics
  bool GetVISkip() const;                           // Used By VideoInterface

  bool UseSyncOnSkipIdle() const;

private:
  Globals m_globals;

  Core::System& m_system;

  // unordered_map stores each element separately as a linked list node so pointers to elements
  // remain stable regardless of rehashes/resizing.
  std::unordered_map<std::string, EventType> m_event_types;

  // STATE_TO_SAVE
  // The queue is a min-heap using std::make_heap/push_heap/pop_heap.
  // We don't use std::priority_queue because we need to be able to serialize, unserialize and
  // erase arbitrary events (RemoveEvent()) regardless of the queue order. These aren't accomodated
  // by the standard adaptor class.
  std::vector<Event> m_event_queue;
  u64 m_event_fifo_id = 0;
  std::mutex m_ts_write_lock;
  Common::SPSCQueue<Event, false> m_ts_queue;

  float m_last_oc_factor = 0.0f;

  s64 m_idled_cycles = 0;
  u32 m_fake_dec_start_value = 0;
  u64 m_fake_dec_start_ticks = 0;

  // Are we in a function that has been called from Advance()
  bool m_is_global_timer_sane = false;

  EventType* m_ev_lost = nullptr;

  CPUThreadConfigCallback::ConfigChangedCallbackID m_registered_config_callback_id;
  float m_config_oc_factor = 0.0f;
  float m_config_oc_inv_factor = 0.0f;
  bool m_config_sync_on_skip_idle = false;

  s64 m_throttle_last_cycle = 0;
  TimePoint m_throttle_deadline = Clock::now();
  s64 m_throttle_clock_per_sec = 0;
  s64 m_throttle_min_clock_per_sleep = 0;
  bool m_throttle_disable_vi_int = false;

  DT m_max_fallback = {};
  double m_emulation_speed = 1.0;

  void ResetThrottle(s64 cycle);

  int DowncountToCycles(int downcount) const;
  int CyclesToDowncount(int cycles) const;
};

}  // namespace CoreTiming
