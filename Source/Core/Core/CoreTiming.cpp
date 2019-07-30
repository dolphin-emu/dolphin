// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/CoreTiming.h"

#include <algorithm>
#include <cinttypes>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/SPSCQueue.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"

#include "VideoCommon/Fifo.h"
#include "VideoCommon/VideoBackendBase.h"

namespace CoreTiming
{
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

// Sort by time, unless the times are the same, in which case sort by the order added to the queue
static bool operator>(const Event& left, const Event& right)
{
  return std::tie(left.time, left.fifo_order) > std::tie(right.time, right.fifo_order);
}
static bool operator<(const Event& left, const Event& right)
{
  return std::tie(left.time, left.fifo_order) < std::tie(right.time, right.fifo_order);
}

// unordered_map stores each element separately as a linked list node so pointers to elements
// remain stable regardless of rehashes/resizing.
static std::unordered_map<std::string, EventType> s_event_types;

// STATE_TO_SAVE
// The queue is a min-heap using std::make_heap/push_heap/pop_heap.
// We don't use std::priority_queue because we need to be able to serialize, unserialize and
// erase arbitrary events (RemoveEvent()) regardless of the queue order. These aren't accomodated
// by the standard adaptor class.
static std::vector<Event> s_event_queue;
static u64 s_event_fifo_id;
static std::mutex s_ts_write_lock;
static Common::SPSCQueue<Event, false> s_ts_queue;

static float s_last_OC_factor;
static constexpr int MAX_SLICE_LENGTH = 20000;

static s64 s_idled_cycles;
static u32 s_fake_dec_start_value;
static u64 s_fake_dec_start_ticks;

// Are we in a function that has been called from Advance()
static bool s_is_global_timer_sane;

Globals g;

static EventType* s_ev_lost = nullptr;

static void EmptyTimedCallback(u64 userdata, s64 cyclesLate)
{
}

// Changing the CPU speed in Dolphin isn't actually done by changing the physical clock rate,
// but by changing the amount of work done in a particular amount of time. This tends to be more
// compatible because it stops the games from actually knowing directly that the clock rate has
// changed, and ensures that anything based on waiting a specific number of cycles still works.
//
// Technically it might be more accurate to call this changing the IPC instead of the CPU speed,
// but the effect is largely the same.
static int DowncountToCycles(int downcount)
{
  return static_cast<int>(downcount * g.last_OC_factor_inverted);
}

static int CyclesToDowncount(int cycles)
{
  return static_cast<int>(cycles * s_last_OC_factor);
}

EventType* RegisterEvent(const std::string& name, TimedCallback callback)
{
  // check for existing type with same name.
  // we want event type names to remain unique so that we can use them for serialization.
  ASSERT_MSG(POWERPC, s_event_types.find(name) == s_event_types.end(),
             "CoreTiming Event \"%s\" is already registered. Events should only be registered "
             "during Init to avoid breaking save states.",
             name.c_str());

  auto info = s_event_types.emplace(name, EventType{callback, nullptr});
  EventType* event_type = &info.first->second;
  event_type->name = &info.first->first;
  return event_type;
}

void UnregisterAllEvents()
{
  ASSERT_MSG(POWERPC, s_event_queue.empty(), "Cannot unregister events with events pending");
  s_event_types.clear();
}

void Init()
{
  s_last_OC_factor = SConfig::GetInstance().m_OCEnable ? SConfig::GetInstance().m_OCFactor : 1.0f;
  g.last_OC_factor_inverted = 1.0f / s_last_OC_factor;
  PowerPC::ppcState.downcount = CyclesToDowncount(MAX_SLICE_LENGTH);
  g.slice_length = MAX_SLICE_LENGTH;
  g.global_timer = 0;
  s_idled_cycles = 0;

  // The time between CoreTiming being intialized and the first call to Advance() is considered
  // the slice boundary between slice -1 and slice 0. Dispatcher loops must call Advance() before
  // executing the first PPC cycle of each slice to prepare the slice length and downcount for
  // that slice.
  s_is_global_timer_sane = true;

  s_event_fifo_id = 0;
  s_ev_lost = RegisterEvent("_lost_event", &EmptyTimedCallback);
}

void Shutdown()
{
  std::lock_guard<std::mutex> lk(s_ts_write_lock);
  MoveEvents();
  ClearPendingEvents();
  UnregisterAllEvents();
}

void DoState(PointerWrap& p)
{
  std::lock_guard<std::mutex> lk(s_ts_write_lock);
  p.Do(g.slice_length);
  p.Do(g.global_timer);
  p.Do(s_idled_cycles);
  p.Do(s_fake_dec_start_value);
  p.Do(s_fake_dec_start_ticks);
  p.Do(g.fake_TB_start_value);
  p.Do(g.fake_TB_start_ticks);
  p.Do(s_last_OC_factor);
  g.last_OC_factor_inverted = 1.0f / s_last_OC_factor;
  p.Do(s_event_fifo_id);

  p.DoMarker("CoreTimingData");

  MoveEvents();
  p.DoEachElement(s_event_queue, [](PointerWrap& pw, Event& ev) {
    pw.Do(ev.time);
    pw.Do(ev.fifo_order);

    // this is why we can't have (nice things) pointers as userdata
    pw.Do(ev.userdata);

    // we can't savestate ev.type directly because events might not get registered in the same
    // order (or at all) every time.
    // so, we savestate the event's type's name, and derive ev.type from that when loading.
    std::string name;
    if (pw.GetMode() != PointerWrap::MODE_READ)
      name = *ev.type->name;

    pw.Do(name);
    if (pw.GetMode() == PointerWrap::MODE_READ)
    {
      auto itr = s_event_types.find(name);
      if (itr != s_event_types.end())
      {
        ev.type = &itr->second;
      }
      else
      {
        WARN_LOG(POWERPC,
                 "Lost event from savestate because its type, \"%s\", has not been registered.",
                 name.c_str());
        ev.type = s_ev_lost;
      }
    }
  });
  p.DoMarker("CoreTimingEvents");

  // When loading from a save state, we must assume the Event order is random and meaningless.
  // The exact layout of the heap in memory is implementation defined, therefore it is platform
  // and library version specific.
  if (p.GetMode() == PointerWrap::MODE_READ)
    std::make_heap(s_event_queue.begin(), s_event_queue.end(), std::greater<Event>());
}

// This should only be called from the CPU thread. If you are calling
// it from any other thread, you are doing something evil
u64 GetTicks()
{
  u64 ticks = static_cast<u64>(g.global_timer);
  if (!s_is_global_timer_sane)
  {
    int downcount = DowncountToCycles(PowerPC::ppcState.downcount);
    ticks += g.slice_length - downcount;
  }
  return ticks;
}

u64 GetIdleTicks()
{
  return static_cast<u64>(s_idled_cycles);
}

void ClearPendingEvents()
{
  s_event_queue.clear();
}

void ScheduleEvent(s64 cycles_into_future, EventType* event_type, u64 userdata, FromThread from)
{
  ASSERT_MSG(POWERPC, event_type, "Event type is nullptr, will crash now.");

  bool from_cpu_thread;
  if (from == FromThread::ANY)
  {
    from_cpu_thread = Core::IsCPUThread();
  }
  else
  {
    from_cpu_thread = from == FromThread::CPU;
    ASSERT_MSG(POWERPC, from_cpu_thread == Core::IsCPUThread(),
               "A \"%s\" event was scheduled from the wrong thread (%s)", event_type->name->c_str(),
               from_cpu_thread ? "CPU" : "non-CPU");
  }

  if (from_cpu_thread)
  {
    s64 timeout = GetTicks() + cycles_into_future;

    // If this event needs to be scheduled before the next advance(), force one early
    if (!s_is_global_timer_sane)
      ForceExceptionCheck(cycles_into_future);

    s_event_queue.emplace_back(Event{timeout, s_event_fifo_id++, userdata, event_type});
    std::push_heap(s_event_queue.begin(), s_event_queue.end(), std::greater<Event>());
  }
  else
  {
    if (Core::WantsDeterminism())
    {
      ERROR_LOG(POWERPC,
                "Someone scheduled an off-thread \"%s\" event while netplay or "
                "movie play/record was active.  This is likely to cause a desync.",
                event_type->name->c_str());
    }

    std::lock_guard<std::mutex> lk(s_ts_write_lock);
    s_ts_queue.Push(Event{g.global_timer + cycles_into_future, 0, userdata, event_type});
  }
}

void RemoveEvent(EventType* event_type)
{
  auto itr = std::remove_if(s_event_queue.begin(), s_event_queue.end(),
                            [&](const Event& e) { return e.type == event_type; });

  // Removing random items breaks the invariant so we have to re-establish it.
  if (itr != s_event_queue.end())
  {
    s_event_queue.erase(itr, s_event_queue.end());
    std::make_heap(s_event_queue.begin(), s_event_queue.end(), std::greater<Event>());
  }
}

void RemoveAllEvents(EventType* event_type)
{
  MoveEvents();
  RemoveEvent(event_type);
}

void ForceExceptionCheck(s64 cycles)
{
  cycles = std::max<s64>(0, cycles);
  if (DowncountToCycles(PowerPC::ppcState.downcount) > cycles)
  {
    // downcount is always (much) smaller than MAX_INT so we can safely cast cycles to an int here.
    // Account for cycles already executed by adjusting the g.slice_length
    g.slice_length -= DowncountToCycles(PowerPC::ppcState.downcount) - static_cast<int>(cycles);
    PowerPC::ppcState.downcount = CyclesToDowncount(static_cast<int>(cycles));
  }
}

void MoveEvents()
{
  for (Event ev; s_ts_queue.Pop(ev);)
  {
    ev.fifo_order = s_event_fifo_id++;
    s_event_queue.emplace_back(std::move(ev));
    std::push_heap(s_event_queue.begin(), s_event_queue.end(), std::greater<Event>());
  }
}

void Advance()
{
  MoveEvents();

  int cyclesExecuted = g.slice_length - DowncountToCycles(PowerPC::ppcState.downcount);
  g.global_timer += cyclesExecuted;
  s_last_OC_factor = SConfig::GetInstance().m_OCEnable ? SConfig::GetInstance().m_OCFactor : 1.0f;
  g.last_OC_factor_inverted = 1.0f / s_last_OC_factor;
  g.slice_length = MAX_SLICE_LENGTH;

  s_is_global_timer_sane = true;

  while (!s_event_queue.empty())
  {
    Event evt = s_event_queue.front();
    if (evt.time <= g.global_timer)
    {
      std::pop_heap(s_event_queue.begin(), s_event_queue.end(), std::greater<Event>());
      s_event_queue.pop_back();
      evt.type->callback(evt.userdata, g.global_timer - evt.time);
    }
    else
    {
      // Still events left (scheduled in the future)
      g.slice_length = static_cast<int>(std::min<s64>(evt.time - g.global_timer, MAX_SLICE_LENGTH));
      break;
    }
  }

  s_is_global_timer_sane = false;

  PowerPC::ppcState.downcount = CyclesToDowncount(g.slice_length);

  // Check for any external exceptions.
  // It's important to do this after processing events otherwise any exceptions will be delayed
  // until the next slice:
  //        Pokemon Box refuses to boot if the first exception from the audio DMA is received late
  PowerPC::CheckExternalExceptions();
}

void LogPendingEvents()
{
  auto clone = s_event_queue;
  std::sort(clone.begin(), clone.end());
  for (const Event& ev : clone)
  {
    INFO_LOG(POWERPC, "PENDING: Now: %" PRId64 " Pending: %" PRId64 " Type: %s", g.global_timer,
             ev.time, ev.type->name->c_str());
  }
}

// Should only be called from the CPU thread after the PPC clock has changed
void AdjustEventQueueTimes(u32 new_ppc_clock, u32 old_ppc_clock)
{
  for (Event& ev : s_event_queue)
  {
    const s64 ticks = (ev.time - g.global_timer) * new_ppc_clock / old_ppc_clock;
    ev.time = g.global_timer + ticks;
  }
}

void Idle()
{
  if (SConfig::GetInstance().bSyncGPUOnSkipIdleHack)
  {
    // When the FIFO is processing data we must not advance because in this way
    // the VI will be desynchronized. So, We are waiting until the FIFO finish and
    // while we process only the events required by the FIFO.
    Fifo::FlushGpu();
  }

  s_idled_cycles += DowncountToCycles(PowerPC::ppcState.downcount);
  PowerPC::ppcState.downcount = 0;
}

std::string GetScheduledEventsSummary()
{
  std::string text = "Scheduled events\n";
  text.reserve(1000);

  auto clone = s_event_queue;
  std::sort(clone.begin(), clone.end());
  for (const Event& ev : clone)
  {
    text += StringFromFormat("%s : %" PRIi64 " %016" PRIx64 "\n", ev.type->name->c_str(), ev.time,
                             ev.userdata);
  }
  return text;
}

u32 GetFakeDecStartValue()
{
  return s_fake_dec_start_value;
}

void SetFakeDecStartValue(u32 val)
{
  s_fake_dec_start_value = val;
}

u64 GetFakeDecStartTicks()
{
  return s_fake_dec_start_ticks;
}

void SetFakeDecStartTicks(u64 val)
{
  s_fake_dec_start_ticks = val;
}

u64 GetFakeTBStartValue()
{
  return g.fake_TB_start_value;
}

void SetFakeTBStartValue(u64 val)
{
  g.fake_TB_start_value = val;
}

u64 GetFakeTBStartTicks()
{
  return g.fake_TB_start_ticks;
}

void SetFakeTBStartTicks(u64 val)
{
  g.fake_TB_start_ticks = val;
}

}  // namespace CoreTiming
