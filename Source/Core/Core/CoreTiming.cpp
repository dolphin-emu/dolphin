// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/CoreTiming.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/SPSCQueue.h"

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "VideoCommon/Fifo.h"
#include "VideoCommon/VideoBackendBase.h"

namespace CoreTiming
{
// Sort by time, unless the times are the same, in which case sort by the order added to the queue
static bool operator>(const Event& left, const Event& right)
{
  return std::tie(left.time, left.fifo_order) > std::tie(right.time, right.fifo_order);
}
static bool operator<(const Event& left, const Event& right)
{
  return std::tie(left.time, left.fifo_order) < std::tie(right.time, right.fifo_order);
}

static constexpr int MAX_SLICE_LENGTH = 20000;

static void EmptyTimedCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
}

// Changing the CPU speed in Dolphin isn't actually done by changing the physical clock rate,
// but by changing the amount of work done in a particular amount of time. This tends to be more
// compatible because it stops the games from actually knowing directly that the clock rate has
// changed, and ensures that anything based on waiting a specific number of cycles still works.
//
// Technically it might be more accurate to call this changing the IPC instead of the CPU speed,
// but the effect is largely the same.
int CoreTimingManager::DowncountToCycles(int downcount) const
{
  return static_cast<int>(downcount * m_globals.last_OC_factor_inverted);
}

int CoreTimingManager::CyclesToDowncount(int cycles) const
{
  return static_cast<int>(cycles * m_last_oc_factor);
}

EventType* CoreTimingManager::RegisterEvent(const std::string& name, TimedCallback callback)
{
  // check for existing type with same name.
  // we want event type names to remain unique so that we can use them for serialization.
  ASSERT_MSG(POWERPC, m_event_types.find(name) == m_event_types.end(),
             "CoreTiming Event \"{}\" is already registered. Events should only be registered "
             "during Init to avoid breaking save states.",
             name);

  auto info = m_event_types.emplace(name, EventType{callback, nullptr});
  EventType* event_type = &info.first->second;
  event_type->name = &info.first->first;
  return event_type;
}

void CoreTimingManager::UnregisterAllEvents()
{
  ASSERT_MSG(POWERPC, m_event_queue.empty(), "Cannot unregister events with events pending");
  m_event_types.clear();
}

void CoreTimingManager::Init()
{
  m_registered_config_callback_id = Config::AddConfigChangedCallback(
      [this]() { Core::RunAsCPUThread([this]() { RefreshConfig(); }); });
  RefreshConfig();

  m_last_oc_factor = m_config_oc_factor;
  m_globals.last_OC_factor_inverted = m_config_oc_inv_factor;
  PowerPC::ppcState.downcount = CyclesToDowncount(MAX_SLICE_LENGTH);
  m_globals.slice_length = MAX_SLICE_LENGTH;
  m_globals.global_timer = 0;
  m_idled_cycles = 0;

  // The time between CoreTiming being intialized and the first call to Advance() is considered
  // the slice boundary between slice -1 and slice 0. Dispatcher loops must call Advance() before
  // executing the first PPC cycle of each slice to prepare the slice length and downcount for
  // that slice.
  m_is_global_timer_sane = true;

  m_event_fifo_id = 0;
  m_ev_lost = RegisterEvent("_lost_event", &EmptyTimedCallback);
}

void CoreTimingManager::Shutdown()
{
  std::lock_guard lk(m_ts_write_lock);
  MoveEvents();
  ClearPendingEvents();
  UnregisterAllEvents();
  Config::RemoveConfigChangedCallback(m_registered_config_callback_id);
}

void CoreTimingManager::RefreshConfig()
{
  m_config_oc_factor =
      Config::Get(Config::MAIN_OVERCLOCK_ENABLE) ? Config::Get(Config::MAIN_OVERCLOCK) : 1.0f;
  m_config_oc_inv_factor = 1.0f / m_config_oc_factor;
  m_config_sync_on_skip_idle = Config::Get(Config::MAIN_SYNC_ON_SKIP_IDLE);
}

void CoreTimingManager::DoState(PointerWrap& p)
{
  std::lock_guard lk(m_ts_write_lock);
  p.Do(m_globals.slice_length);
  p.Do(m_globals.global_timer);
  p.Do(m_idled_cycles);
  p.Do(m_fake_dec_start_value);
  p.Do(m_fake_dec_start_ticks);
  p.Do(m_globals.fake_TB_start_value);
  p.Do(m_globals.fake_TB_start_ticks);
  p.Do(m_last_oc_factor);
  m_globals.last_OC_factor_inverted = 1.0f / m_last_oc_factor;
  p.Do(m_event_fifo_id);

  p.DoMarker("CoreTimingData");

  MoveEvents();
  p.DoEachElement(m_event_queue, [this](PointerWrap& pw, Event& ev) {
    pw.Do(ev.time);
    pw.Do(ev.fifo_order);

    // this is why we can't have (nice things) pointers as userdata
    pw.Do(ev.userdata);

    // we can't savestate ev.type directly because events might not get registered in the same
    // order (or at all) every time.
    // so, we savestate the event's type's name, and derive ev.type from that when loading.
    std::string name;
    if (!pw.IsReadMode())
      name = *ev.type->name;

    pw.Do(name);
    if (pw.IsReadMode())
    {
      auto itr = m_event_types.find(name);
      if (itr != m_event_types.end())
      {
        ev.type = &itr->second;
      }
      else
      {
        WARN_LOG_FMT(POWERPC,
                     "Lost event from savestate because its type, \"{}\", has not been registered.",
                     name);
        ev.type = m_ev_lost;
      }
    }
  });
  p.DoMarker("CoreTimingEvents");

  // When loading from a save state, we must assume the Event order is random and meaningless.
  // The exact layout of the heap in memory is implementation defined, therefore it is platform
  // and library version specific.
  if (p.IsReadMode())
    std::make_heap(m_event_queue.begin(), m_event_queue.end(), std::greater<Event>());
}

// This should only be called from the CPU thread. If you are calling
// it from any other thread, you are doing something evil
u64 CoreTimingManager::GetTicks() const
{
  u64 ticks = static_cast<u64>(m_globals.global_timer);
  if (!m_is_global_timer_sane)
  {
    int downcount = DowncountToCycles(PowerPC::ppcState.downcount);
    ticks += m_globals.slice_length - downcount;
  }
  return ticks;
}

u64 CoreTimingManager::GetIdleTicks() const
{
  return static_cast<u64>(m_idled_cycles);
}

void CoreTimingManager::ClearPendingEvents()
{
  m_event_queue.clear();
}

void CoreTimingManager::ScheduleEvent(s64 cycles_into_future, EventType* event_type, u64 userdata,
                                      FromThread from)
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
               "A \"{}\" event was scheduled from the wrong thread ({})", *event_type->name,
               from_cpu_thread ? "CPU" : "non-CPU");
  }

  if (from_cpu_thread)
  {
    s64 timeout = GetTicks() + cycles_into_future;

    // If this event needs to be scheduled before the next advance(), force one early
    if (!m_is_global_timer_sane)
      ForceExceptionCheck(cycles_into_future);

    m_event_queue.emplace_back(Event{timeout, m_event_fifo_id++, userdata, event_type});
    std::push_heap(m_event_queue.begin(), m_event_queue.end(), std::greater<Event>());
  }
  else
  {
    if (Core::WantsDeterminism())
    {
      ERROR_LOG_FMT(POWERPC,
                    "Someone scheduled an off-thread \"{}\" event while netplay or "
                    "movie play/record was active.  This is likely to cause a desync.",
                    *event_type->name);
    }

    std::lock_guard lk(m_ts_write_lock);
    m_ts_queue.Push(Event{m_globals.global_timer + cycles_into_future, 0, userdata, event_type});
  }
}

void CoreTimingManager::RemoveEvent(EventType* event_type)
{
  auto itr = std::remove_if(m_event_queue.begin(), m_event_queue.end(),
                            [&](const Event& e) { return e.type == event_type; });

  // Removing random items breaks the invariant so we have to re-establish it.
  if (itr != m_event_queue.end())
  {
    m_event_queue.erase(itr, m_event_queue.end());
    std::make_heap(m_event_queue.begin(), m_event_queue.end(), std::greater<Event>());
  }
}

void CoreTimingManager::RemoveAllEvents(EventType* event_type)
{
  MoveEvents();
  RemoveEvent(event_type);
}

void CoreTimingManager::ForceExceptionCheck(s64 cycles)
{
  cycles = std::max<s64>(0, cycles);
  if (DowncountToCycles(PowerPC::ppcState.downcount) > cycles)
  {
    // downcount is always (much) smaller than MAX_INT so we can safely cast cycles to an int here.
    // Account for cycles already executed by adjusting the m_globals.slice_length
    m_globals.slice_length -=
        DowncountToCycles(PowerPC::ppcState.downcount) - static_cast<int>(cycles);
    PowerPC::ppcState.downcount = CyclesToDowncount(static_cast<int>(cycles));
  }
}

void CoreTimingManager::MoveEvents()
{
  for (Event ev; m_ts_queue.Pop(ev);)
  {
    ev.fifo_order = m_event_fifo_id++;
    m_event_queue.emplace_back(std::move(ev));
    std::push_heap(m_event_queue.begin(), m_event_queue.end(), std::greater<Event>());
  }
}

void CoreTimingManager::Advance()
{
  auto& system = Core::System::GetInstance();

  MoveEvents();

  int cyclesExecuted = m_globals.slice_length - DowncountToCycles(PowerPC::ppcState.downcount);
  m_globals.global_timer += cyclesExecuted;
  m_last_oc_factor = m_config_oc_factor;
  m_globals.last_OC_factor_inverted = m_config_oc_inv_factor;
  m_globals.slice_length = MAX_SLICE_LENGTH;

  m_is_global_timer_sane = true;

  while (!m_event_queue.empty() && m_event_queue.front().time <= m_globals.global_timer)
  {
    Event evt = std::move(m_event_queue.front());
    std::pop_heap(m_event_queue.begin(), m_event_queue.end(), std::greater<Event>());
    m_event_queue.pop_back();
    evt.type->callback(system, evt.userdata, m_globals.global_timer - evt.time);
  }

  m_is_global_timer_sane = false;

  // Still events left (scheduled in the future)
  if (!m_event_queue.empty())
  {
    m_globals.slice_length = static_cast<int>(
        std::min<s64>(m_event_queue.front().time - m_globals.global_timer, MAX_SLICE_LENGTH));
  }

  PowerPC::ppcState.downcount = CyclesToDowncount(m_globals.slice_length);

  // Check for any external exceptions.
  // It's important to do this after processing events otherwise any exceptions will be delayed
  // until the next slice:
  //        Pokemon Box refuses to boot if the first exception from the audio DMA is received late
  PowerPC::CheckExternalExceptions();
}

void CoreTimingManager::LogPendingEvents() const
{
  auto clone = m_event_queue;
  std::sort(clone.begin(), clone.end());
  for (const Event& ev : clone)
  {
    INFO_LOG_FMT(POWERPC, "PENDING: Now: {} Pending: {} Type: {}", m_globals.global_timer, ev.time,
                 *ev.type->name);
  }
}

// Should only be called from the CPU thread after the PPC clock has changed
void CoreTimingManager::AdjustEventQueueTimes(u32 new_ppc_clock, u32 old_ppc_clock)
{
  for (Event& ev : m_event_queue)
  {
    const s64 ticks = (ev.time - m_globals.global_timer) * new_ppc_clock / old_ppc_clock;
    ev.time = m_globals.global_timer + ticks;
  }
}

void CoreTimingManager::Idle()
{
  if (m_config_sync_on_skip_idle)
  {
    // When the FIFO is processing data we must not advance because in this way
    // the VI will be desynchronized. So, We are waiting until the FIFO finish and
    // while we process only the events required by the FIFO.
    Fifo::FlushGpu();
  }

  PowerPC::UpdatePerformanceMonitor(PowerPC::ppcState.downcount, 0, 0);
  m_idled_cycles += DowncountToCycles(PowerPC::ppcState.downcount);
  PowerPC::ppcState.downcount = 0;
}

std::string CoreTimingManager::GetScheduledEventsSummary() const
{
  std::string text = "Scheduled events\n";
  text.reserve(1000);

  auto clone = m_event_queue;
  std::sort(clone.begin(), clone.end());
  for (const Event& ev : clone)
  {
    text += fmt::format("{} : {} {:016x}\n", *ev.type->name, ev.time, ev.userdata);
  }
  return text;
}

u32 CoreTimingManager::GetFakeDecStartValue() const
{
  return m_fake_dec_start_value;
}

void CoreTimingManager::SetFakeDecStartValue(u32 val)
{
  m_fake_dec_start_value = val;
}

u64 CoreTimingManager::GetFakeDecStartTicks() const
{
  return m_fake_dec_start_ticks;
}

void CoreTimingManager::SetFakeDecStartTicks(u64 val)
{
  m_fake_dec_start_ticks = val;
}

u64 CoreTimingManager::GetFakeTBStartValue() const
{
  return m_globals.fake_TB_start_value;
}

void CoreTimingManager::SetFakeTBStartValue(u64 val)
{
  m_globals.fake_TB_start_value = val;
}

u64 CoreTimingManager::GetFakeTBStartTicks() const
{
  return m_globals.fake_TB_start_ticks;
}

void CoreTimingManager::SetFakeTBStartTicks(u64 val)
{
  m_globals.fake_TB_start_ticks = val;
}

void GlobalAdvance()
{
  Core::System::GetInstance().GetCoreTiming().Advance();
}

void GlobalIdle()
{
  Core::System::GetInstance().GetCoreTiming().Idle();
}

}  // namespace CoreTiming
