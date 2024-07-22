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

#include "Core/AchievementManager.h"
#include "Core/CPUThreadConfigCallback.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "VideoCommon/Fifo.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PerformanceMetrics.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

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

CoreTimingManager::CoreTimingManager(Core::System& system) : m_system(system)
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
  ASSERT_MSG(POWERPC, !m_event_types.contains(name),
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
  m_registered_config_callback_id =
      CPUThreadConfigCallback::AddConfigChangedCallback([this]() { RefreshConfig(); });
  RefreshConfig();

  m_last_oc_factor = m_config_oc_factor;
  m_globals.last_OC_factor_inverted = m_config_oc_inv_factor;
  m_system.GetPPCState().downcount = CyclesToDowncount(MAX_SLICE_LENGTH);
  m_globals.slice_length = MAX_SLICE_LENGTH;
  m_globals.global_timer = 0;
  m_idled_cycles = 0;

  // The time between CoreTiming being intialized and the first call to Advance() is considered
  // the slice boundary between slice -1 and slice 0. Dispatcher loops must call Advance() before
  // executing the first PPC cycle of each slice to prepare the slice length and downcount for
  // that slice.
  m_is_global_timer_sane = true;

  // Reset data used by the throttling system
  ResetThrottle(0);

  m_event_fifo_id = 0;
  m_ev_lost = RegisterEvent("_lost_event", &EmptyTimedCallback);
}

void CoreTimingManager::Shutdown()
{
  std::lock_guard lk(m_ts_write_lock);
  MoveEvents();
  ClearPendingEvents();
  UnregisterAllEvents();
  CPUThreadConfigCallback::RemoveConfigChangedCallback(m_registered_config_callback_id);
}

void CoreTimingManager::RefreshConfig()
{
  m_config_oc_factor =
      Config::Get(Config::MAIN_OVERCLOCK_ENABLE) ? Config::Get(Config::MAIN_OVERCLOCK) : 1.0f;
  m_config_oc_inv_factor = 1.0f / m_config_oc_factor;
  m_config_sync_on_skip_idle = Config::Get(Config::MAIN_SYNC_ON_SKIP_IDLE);

  // A maximum fallback is used to prevent the system from sleeping for
  // too long or going full speed in an attempt to catch up to timings.
  m_max_fallback = std::chrono::duration_cast<DT>(DT_ms(Config::Get(Config::MAIN_MAX_FALLBACK)));

  m_max_variance = std::chrono::duration_cast<DT>(DT_ms(Config::Get(Config::MAIN_TIMING_VARIANCE)));

  if (AchievementManager::GetInstance().IsHardcoreModeActive() &&
      Config::Get(Config::MAIN_EMULATION_SPEED) < 1.0f &&
      Config::Get(Config::MAIN_EMULATION_SPEED) > 0.0f)
  {
    Config::SetCurrent(Config::MAIN_EMULATION_SPEED, 1.0f);
    m_emulation_speed = 1.0f;
    OSD::AddMessage("Minimum speed is 100% in Hardcore Mode");
  }

  m_emulation_speed = Config::Get(Config::MAIN_EMULATION_SPEED);
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

  if (p.IsReadMode())
  {
    // When loading from a save state, we must assume the Event order is random and meaningless.
    // The exact layout of the heap in memory is implementation defined, therefore it is platform
    // and library version specific.
    std::ranges::make_heap(m_event_queue, std::greater<Event>());

    // The stave state has changed the time, so our previous Throttle targets are invalid.
    // Especially when global_time goes down; So we create a fake throttle update.
    ResetThrottle(m_globals.global_timer);
  }
}

// This should only be called from the CPU thread. If you are calling
// it from any other thread, you are doing something evil
u64 CoreTimingManager::GetTicks() const
{
  u64 ticks = static_cast<u64>(m_globals.global_timer);
  if (!m_is_global_timer_sane)
  {
    int downcount = DowncountToCycles(m_system.GetPPCState().downcount);
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
    std::ranges::push_heap(m_event_queue, std::greater<Event>());
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
  const size_t erased =
      std::erase_if(m_event_queue, [&](const Event& e) { return e.type == event_type; });

  // Removing random items breaks the invariant so we have to re-establish it.
  if (erased != 0)
  {
    std::ranges::make_heap(m_event_queue, std::greater<Event>());
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
  auto& ppc_state = m_system.GetPPCState();
  if (DowncountToCycles(ppc_state.downcount) > cycles)
  {
    // downcount is always (much) smaller than MAX_INT so we can safely cast cycles to an int here.
    // Account for cycles already executed by adjusting the m_globals.slice_length
    m_globals.slice_length -= DowncountToCycles(ppc_state.downcount) - static_cast<int>(cycles);
    ppc_state.downcount = CyclesToDowncount(static_cast<int>(cycles));
  }
}

void CoreTimingManager::MoveEvents()
{
  for (Event ev; m_ts_queue.Pop(ev);)
  {
    ev.fifo_order = m_event_fifo_id++;
    m_event_queue.emplace_back(std::move(ev));
    std::ranges::push_heap(m_event_queue, std::greater<Event>());
  }
}

void CoreTimingManager::Advance()
{
  CPUThreadConfigCallback::CheckForConfigChanges();

  MoveEvents();

  auto& power_pc = m_system.GetPowerPC();
  auto& ppc_state = power_pc.GetPPCState();

  int cyclesExecuted = m_globals.slice_length - DowncountToCycles(ppc_state.downcount);
  m_globals.global_timer += cyclesExecuted;
  m_last_oc_factor = m_config_oc_factor;
  m_globals.last_OC_factor_inverted = m_config_oc_inv_factor;
  m_globals.slice_length = MAX_SLICE_LENGTH;

  m_is_global_timer_sane = true;

  while (!m_event_queue.empty() && m_event_queue.front().time <= m_globals.global_timer)
  {
    Event evt = std::move(m_event_queue.front());
    std::ranges::pop_heap(m_event_queue, std::greater<Event>());
    m_event_queue.pop_back();

    Throttle(evt.time);
    evt.type->callback(m_system, evt.userdata, m_globals.global_timer - evt.time);
  }

  m_is_global_timer_sane = false;

  // Still events left (scheduled in the future)
  if (!m_event_queue.empty())
  {
    m_globals.slice_length = static_cast<int>(
        std::min<s64>(m_event_queue.front().time - m_globals.global_timer, MAX_SLICE_LENGTH));
  }

  ppc_state.downcount = CyclesToDowncount(m_globals.slice_length);

  // Check for any external exceptions.
  // It's important to do this after processing events otherwise any exceptions will be delayed
  // until the next slice:
  //        Pokemon Box refuses to boot if the first exception from the audio DMA is received late
  power_pc.CheckExternalExceptions();
}

void CoreTimingManager::Throttle(const s64 target_cycle)
{
  // Based on number of cycles and emulation speed, increase the target deadline
  const s64 cycles = target_cycle - m_throttle_last_cycle;

  // Prevent any throttling code if the amount of time passed is < ~0.122ms
  if (cycles < m_throttle_min_clock_per_sleep)
    return;

  m_throttle_last_cycle = target_cycle;

  const double speed = Core::GetIsThrottlerTempDisabled() ? 0.0 : m_emulation_speed;

  if (0.0 < speed)
    m_throttle_deadline +=
        std::chrono::duration_cast<DT>(DT_s(cycles) / (speed * m_throttle_clock_per_sec));

  const TimePoint time = Clock::now();
  const TimePoint min_deadline = time - m_max_fallback;
  const TimePoint max_deadline = time + m_max_fallback;

  if (m_throttle_deadline > max_deadline)
  {
    m_throttle_deadline = max_deadline;
  }
  else if (m_throttle_deadline < min_deadline)
  {
    DEBUG_LOG_FMT(COMMON, "System can not to keep up with timings! [relaxing timings by {} us]",
                  DT_us(min_deadline - m_throttle_deadline).count());
    m_throttle_deadline = min_deadline;
  }

  const TimePoint vi_deadline = time - std::min(m_max_fallback, m_max_variance) / 2;

  // Skip the VI interrupt if the CPU is lagging by a certain amount.
  // It doesn't matter what amount of lag we skip VI at, as long as it's constant.
  m_throttle_disable_vi_int = 0.0 < speed && m_throttle_deadline < vi_deadline;

  // Only sleep if we are behind the deadline
  if (time < m_throttle_deadline)
  {
    std::this_thread::sleep_until(m_throttle_deadline);

    // Count amount of time sleeping for analytics
    const TimePoint time_after_sleep = Clock::now();
    g_perf_metrics.CountThrottleSleep(time_after_sleep - time);
  }
}

void CoreTimingManager::ResetThrottle(s64 cycle)
{
  m_throttle_last_cycle = cycle;
  m_throttle_deadline = Clock::now();
}

TimePoint CoreTimingManager::GetCPUTimePoint(s64 cyclesLate) const
{
  return TimePoint(std::chrono::duration_cast<DT>(DT_s(m_globals.global_timer - cyclesLate) /
                                                  m_throttle_clock_per_sec));
}

bool CoreTimingManager::GetVISkip() const
{
  return m_throttle_disable_vi_int && g_ActiveConfig.bVISkip && !Core::WantsDeterminism();
}

bool CoreTimingManager::UseSyncOnSkipIdle() const
{
  return m_config_sync_on_skip_idle;
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
  m_throttle_clock_per_sec = new_ppc_clock;
  m_throttle_min_clock_per_sleep = new_ppc_clock / 1200;

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
    m_system.GetFifo().FlushGpu();
  }

  auto& ppc_state = m_system.GetPPCState();
  PowerPC::UpdatePerformanceMonitor(ppc_state.downcount, 0, 0, ppc_state);
  m_idled_cycles += DowncountToCycles(ppc_state.downcount);
  ppc_state.downcount = 0;
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
