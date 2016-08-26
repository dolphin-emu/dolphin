// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <mutex>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/FifoQueue.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"

#include "VideoCommon/Fifo.h"
#include "VideoCommon/VideoBackendBase.h"

namespace CoreTiming
{
struct EventType
{
  TimedCallback callback;
  std::string name;
};

static std::vector<EventType> s_event_types;

struct BaseEvent
{
  s64 time;
  u64 userdata;
  int type;
};

typedef LinkedListItem<BaseEvent> Event;

// STATE_TO_SAVE
static Event* s_first_event;
static std::mutex s_ts_write_lock;
static Common::FifoQueue<BaseEvent, false> s_ts_queue;

// event pools
static Event* s_event_pool = nullptr;

static float s_last_OC_factor;
float g_last_OC_factor_inverted;
int g_slice_length;
static constexpr int MAX_SLICE_LENGTH = 20000;

static s64 s_idled_cycles;
static u32 s_fake_dec_start_value;
static u64 s_fake_dec_start_ticks;

// Are we in a function that has been called from Advance()
static bool s_is_global_timer_sane;

s64 g_global_timer;
u64 g_fake_TB_start_value;
u64 g_fake_TB_start_ticks;

static int s_ev_lost;

static Event* GetNewEvent()
{
  if (!s_event_pool)
    return new Event;

  Event* ev = s_event_pool;
  s_event_pool = ev->next;
  return ev;
}

static void FreeEvent(Event* ev)
{
  ev->next = s_event_pool;
  s_event_pool = ev;
}

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
  return static_cast<int>(downcount * g_last_OC_factor_inverted);
}

static int CyclesToDowncount(int cycles)
{
  return static_cast<int>(cycles * s_last_OC_factor);
}

int RegisterEvent(const std::string& name, TimedCallback callback)
{
  EventType type;
  type.name = name;
  type.callback = callback;

  // check for existing type with same name.
  // we want event type names to remain unique so that we can use them for serialization.
  for (auto& event_type : s_event_types)
  {
    if (name == event_type.name)
    {
      WARN_LOG(
          POWERPC,
          "Discarded old event type \"%s\" because a new type with the same name was registered.",
          name.c_str());
      // we don't know if someone might be holding on to the type index,
      // so we gut the old event type instead of actually removing it.
      event_type.name = "_discarded_event";
      event_type.callback = &EmptyTimedCallback;
    }
  }

  s_event_types.push_back(type);
  return static_cast<int>(s_event_types.size() - 1);
}

void UnregisterAllEvents()
{
  if (s_first_event)
    PanicAlert("Cannot unregister events with events pending");
  s_event_types.clear();
}

void Init()
{
  s_last_OC_factor = SConfig::GetInstance().m_OCEnable ? SConfig::GetInstance().m_OCFactor : 1.0f;
  g_last_OC_factor_inverted = 1.0f / s_last_OC_factor;
  PowerPC::ppcState.downcount = CyclesToDowncount(MAX_SLICE_LENGTH);
  g_slice_length = MAX_SLICE_LENGTH;
  g_global_timer = 0;
  s_idled_cycles = 0;
  s_is_global_timer_sane = true;

  s_ev_lost = RegisterEvent("_lost_event", &EmptyTimedCallback);
}

void Shutdown()
{
  std::lock_guard<std::mutex> lk(s_ts_write_lock);
  MoveEvents();
  ClearPendingEvents();
  UnregisterAllEvents();

  while (s_event_pool)
  {
    Event* ev = s_event_pool;
    s_event_pool = ev->next;
    delete ev;
  }
}

static void EventDoState(PointerWrap& p, BaseEvent* ev)
{
  p.Do(ev->time);

  // this is why we can't have (nice things) pointers as userdata
  p.Do(ev->userdata);

  // we can't savestate ev->type directly because events might not get registered in the same order
  // (or at all) every time.
  // so, we savestate the event's type's name, and derive ev->type from that when loading.
  std::string name;
  if (p.GetMode() != PointerWrap::MODE_READ)
    name = s_event_types[ev->type].name;

  p.Do(name);
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    bool foundMatch = false;
    for (unsigned int i = 0; i < s_event_types.size(); ++i)
    {
      if (name == s_event_types[i].name)
      {
        ev->type = i;
        foundMatch = true;
        break;
      }
    }
    if (!foundMatch)
    {
      WARN_LOG(POWERPC,
               "Lost event from savestate because its type, \"%s\", has not been registered.",
               name.c_str());
      ev->type = s_ev_lost;
    }
  }
}

void DoState(PointerWrap& p)
{
  std::lock_guard<std::mutex> lk(s_ts_write_lock);
  p.Do(g_slice_length);
  p.Do(g_global_timer);
  p.Do(s_idled_cycles);
  p.Do(s_fake_dec_start_value);
  p.Do(s_fake_dec_start_ticks);
  p.Do(g_fake_TB_start_value);
  p.Do(g_fake_TB_start_ticks);
  p.Do(s_last_OC_factor);
  g_last_OC_factor_inverted = 1.0f / s_last_OC_factor;

  p.DoMarker("CoreTimingData");

  MoveEvents();

  p.DoLinkedList<BaseEvent, GetNewEvent, FreeEvent, EventDoState>(s_first_event);
  p.DoMarker("CoreTimingEvents");
}

// This should only be called from the CPU thread. If you are calling
// it from any other thread, you are doing something evil
u64 GetTicks()
{
  u64 ticks = static_cast<u64>(g_global_timer);
  if (!s_is_global_timer_sane)
  {
    int downcount = DowncountToCycles(PowerPC::ppcState.downcount);
    ticks += g_slice_length - downcount;
  }
  return ticks;
}

u64 GetIdleTicks()
{
  return static_cast<u64>(s_idled_cycles);
}

void ClearPendingEvents()
{
  while (s_first_event)
  {
    Event* e = s_first_event->next;
    FreeEvent(s_first_event);
    s_first_event = e;
  }
}

static void AddEventToQueue(Event* ne)
{
  Event* prev = nullptr;
  Event** pNext = &s_first_event;
  for (;;)
  {
    Event*& next = *pNext;
    if (!next || ne->time < next->time)
    {
      ne->next = next;
      next = ne;
      break;
    }
    prev = next;
    pNext = &prev->next;
  }
}

void ScheduleEvent(s64 cycles_into_future, int event_type, u64 userdata, FromThread from)
{
  bool from_cpu_thread;
  if (from == FromThread::ANY)
  {
    from_cpu_thread = Core::IsCPUThread();
  }
  else
  {
    from_cpu_thread = from == FromThread::CPU;
    _assert_msg_(POWERPC, from_cpu_thread == Core::IsCPUThread(),
                 "ScheduleEvent from wrong thread (%s)", from_cpu_thread ? "CPU" : "non-CPU");
  }

  if (from_cpu_thread)
  {
    Event* ne = GetNewEvent();
    ne->time = GetTicks() + cycles_into_future;
    ne->userdata = userdata;
    ne->type = event_type;

    // If this event needs to be scheduled before the next advance(), force one early
    if (!s_is_global_timer_sane)
      ForceExceptionCheck(cycles_into_future);

    AddEventToQueue(ne);
  }
  else
  {
    if (Core::g_want_determinism)
    {
      ERROR_LOG(POWERPC, "Someone scheduled an off-thread \"%s\" event while netplay or "
                         "movie play/record was active.  This is likely to cause a desync.",
                s_event_types[event_type].name.c_str());
    }

    std::lock_guard<std::mutex> lk(s_ts_write_lock);
    Event ne;
    ne.time = g_global_timer + cycles_into_future;
    ne.type = event_type;
    ne.userdata = userdata;
    s_ts_queue.Push(ne);
  }
}

void RemoveEvent(int event_type)
{
  while (s_first_event && s_first_event->type == event_type)
  {
    Event* next = s_first_event->next;
    FreeEvent(s_first_event);
    s_first_event = next;
  }

  if (!s_first_event)
    return;

  Event* prev = s_first_event;
  Event* ptr = prev->next;
  while (ptr)
  {
    if (ptr->type == event_type)
    {
      prev->next = ptr->next;
      FreeEvent(ptr);
      ptr = prev->next;
    }
    else
    {
      prev = ptr;
      ptr = ptr->next;
    }
  }
}

void RemoveAllEvents(int event_type)
{
  MoveEvents();
  RemoveEvent(event_type);
}

void ForceExceptionCheck(s64 cycles)
{
  if (DowncountToCycles(PowerPC::ppcState.downcount) > cycles)
  {
    // downcount is always (much) smaller than MAX_INT so we can safely cast cycles to an int here.
    // Account for cycles already executed by adjusting the g_slice_length
    g_slice_length -= (DowncountToCycles(PowerPC::ppcState.downcount) - static_cast<int>(cycles));
    PowerPC::ppcState.downcount = CyclesToDowncount(static_cast<int>(cycles));
  }
}

void MoveEvents()
{
  BaseEvent sevt;
  while (s_ts_queue.Pop(sevt))
  {
    Event* evt = GetNewEvent();
    evt->time = sevt.time;
    evt->userdata = sevt.userdata;
    evt->type = sevt.type;
    AddEventToQueue(evt);
  }
}

void Advance()
{
  MoveEvents();

  int cyclesExecuted = g_slice_length - DowncountToCycles(PowerPC::ppcState.downcount);
  g_global_timer += cyclesExecuted;
  s_last_OC_factor = SConfig::GetInstance().m_OCEnable ? SConfig::GetInstance().m_OCFactor : 1.0f;
  g_last_OC_factor_inverted = 1.0f / s_last_OC_factor;
  g_slice_length = MAX_SLICE_LENGTH;

  s_is_global_timer_sane = true;

  while (s_first_event && s_first_event->time <= g_global_timer)
  {
    // LOG(POWERPC, "[Scheduler] %s     (%lld, %lld) ",
    //             s_event_types[s_first_event->type].name ? s_event_types[s_first_event->type].name
    //             : "?",
    //             (u64)g_global_timer, (u64)s_first_event->time);
    Event* evt = s_first_event;
    s_first_event = s_first_event->next;
    s_event_types[evt->type].callback(evt->userdata, g_global_timer - evt->time);
    FreeEvent(evt);
  }

  s_is_global_timer_sane = false;

  if (s_first_event)
  {
    g_slice_length =
        static_cast<int>(std::min<s64>(s_first_event->time - g_global_timer, MAX_SLICE_LENGTH));
  }

  PowerPC::ppcState.downcount = CyclesToDowncount(g_slice_length);

  // Check for any external exceptions.
  // It's important to do this after processing events otherwise any exceptions will be delayed
  // until the next slice:
  //        Pokemon Box refuses to boot if the first exception from the audio DMA is received late
  PowerPC::CheckExternalExceptions();
}

void LogPendingEvents()
{
  Event* ptr = s_first_event;
  while (ptr)
  {
    INFO_LOG(POWERPC, "PENDING: Now: %" PRId64 " Pending: %" PRId64 " Type: %d", g_global_timer,
             ptr->time, ptr->type);
    ptr = ptr->next;
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
  Event* ptr = s_first_event;
  std::string text = "Scheduled events\n";
  text.reserve(1000);
  while (ptr)
  {
    unsigned int t = ptr->type;
    if (t >= s_event_types.size())
      PanicAlertT("Invalid event type %i", t);

    const std::string& name = s_event_types[ptr->type].name;

    text += StringFromFormat("%s : %" PRIi64 " %016" PRIx64 "\n", name.c_str(), ptr->time,
                             ptr->userdata);
    ptr = ptr->next;
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
  return g_fake_TB_start_value;
}

void SetFakeTBStartValue(u64 val)
{
  g_fake_TB_start_value = val;
}

u64 GetFakeTBStartTicks()
{
  return g_fake_TB_start_ticks;
}

void SetFakeTBStartTicks(u64 val)
{
  g_fake_TB_start_ticks = val;
}

}  // namespace
