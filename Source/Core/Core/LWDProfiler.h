// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

#include <compare>
#include <deque>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

namespace LWDProfiler
{
using Marker = const char*;

struct Entry
{
public:
  Entry(const Marker name, const DT duration, const DT total_duration)
      : name{name}, percentage{DT_s(duration) / DT_s(total_duration)}, duration{duration}
  {
  }

  std::string GetString() const
  {
    if (duration == DT::zero())
      return fmt::format("  DONE - [{}]", name);
    else
      return fmt::format("{:5.2f}% - {}", 100.0 * percentage, name);
  }

  friend auto operator<=>(const Entry& a, const Entry& b) { return a.percentage <=> b.percentage; };

  Marker name;
  double percentage;
  DT duration;
};

class Profiler
{
private:
  struct TimeSlice
  {
  public:
    TimeSlice(DT& marked_total, DT duration = DT::zero())
        : duration{duration}, marked_total{marked_total}
    {
      marked_total += duration;
    }

    ~TimeSlice() { marked_total -= duration; }

    TimeSlice(TimeSlice&&) = delete;
    TimeSlice(const TimeSlice&) = delete;
    TimeSlice& operator=(TimeSlice&&) = delete;
    TimeSlice& operator=(const TimeSlice&) = delete;

    const DT duration;

  private:
    DT& marked_total;
  };

public:
  Profiler() { Reset(); };

  Profiler(const Profiler&) = delete;
  Profiler& operator=(const Profiler&) = delete;

  void Reset(const Marker first_marker = Marker("Nothing"));

  void SetMarker(const Marker next_marker);

  void PushMarker(const Marker next_marker);
  void PopMarker();

  std::vector<Entry> GetEntries(std::size_t max_entries);

private:
  mutable std::recursive_mutex m_profiler_lock;

  DT m_dt_total;
  std::deque<TimeSlice> m_dt_queue;

  std::unordered_map<Marker, DT> m_dt_marked_totals;

  TimePoint m_last_time;
  Marker m_last_marker;

  std::vector<Marker> m_marker_stack;
};

Profiler& GetCPUProfiler();
Profiler& GetGPUProfiler();

Profiler& GetCurrentProfiler();

class ScopedMarker
{
public:
  ScopedMarker(Profiler& profiler, const Marker name) : profiler{profiler}
  {
    profiler.PushMarker(name);
  }

  ScopedMarker(ScopedMarker& scoped_marker, const Marker name)
      : ScopedMarker{scoped_marker.profiler, name}
  {
  }

  ScopedMarker(const Marker name) : ScopedMarker{GetCurrentProfiler(), name} {}

  ~ScopedMarker() { profiler.PopMarker(); }

  ScopedMarker(ScopedMarker&&) = delete;
  ScopedMarker(const ScopedMarker&) = delete;
  ScopedMarker& operator=(ScopedMarker&&) = delete;
  ScopedMarker& operator=(const ScopedMarker&) = delete;

  void SetMarker(const Marker marker) { profiler.SetMarker(marker); }

private:
  Profiler& profiler;
};
}  // namespace LWDProfiler
