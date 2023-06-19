// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Profiler.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <ios>
#include <sstream>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "Common/Timer.h"

namespace Common
{
static constexpr u32 PROFILER_FIELD_LENGTH = 8;
static constexpr u32 PROFILER_FIELD_LENGTH_FP = PROFILER_FIELD_LENGTH + 3;
static constexpr int PROFILER_LAZY_DELAY = 60;  // in frames

std::list<Profiler*> Profiler::s_all_profilers;
std::mutex Profiler::s_mutex;
u32 Profiler::s_max_length = 0;
u64 Profiler::s_frame_time;
u64 Profiler::s_usecs_frame;

std::string Profiler::s_lazy_result;
int Profiler::s_lazy_delay = 0;

Profiler::Profiler(const std::string& name)
    : m_name(name), m_usecs(0), m_usecs_min(UINT64_MAX), m_usecs_max(0), m_usecs_quad(0),
      m_calls(0), m_depth(0)
{
  m_time = Common::Timer::NowUs();
  s_max_length = std::max<u32>(s_max_length, u32(m_name.length()));

  std::lock_guard<std::mutex> lk(s_mutex);
  s_all_profilers.push_back(this);
}

Profiler::~Profiler()
{
  std::lock_guard<std::mutex> lk(s_mutex);
  s_all_profilers.remove(this);
}

bool Profiler::operator<(const Profiler& b) const
{
  return m_usecs < b.m_usecs;
}

std::string Profiler::ToString()
{
  if (s_lazy_delay > 0)
  {
    s_lazy_delay--;
    return s_lazy_result;
  }
  s_lazy_delay = PROFILER_LAZY_DELAY - 1;

  // don't write anything if no profilation is enabled
  std::lock_guard<std::mutex> lk(s_mutex);
  if (s_all_profilers.empty())
    return "";

  u64 end = Common::Timer::NowUs();
  s_usecs_frame = end - s_frame_time;
  s_frame_time = end;

  std::ostringstream buffer;
  fmt::print(buffer, "{:<{}s} {:>{}s} {:>{}s} {:>{}s} {:>{}s} {:>{}s} {:>{}s} {:>{}s} / usec\n",  //
             "", s_max_length,                                                                    //
             "calls", PROFILER_FIELD_LENGTH,                                                      //
             "sum", PROFILER_FIELD_LENGTH,                                                        //
             "rel", PROFILER_FIELD_LENGTH_FP,                                                     //
             "min", PROFILER_FIELD_LENGTH,                                                        //
             "avg", PROFILER_FIELD_LENGTH_FP,                                                     //
             "stdev", PROFILER_FIELD_LENGTH_FP,                                                   //
             "max", PROFILER_FIELD_LENGTH);                                                       //

  s_all_profilers.sort([](Profiler* a, Profiler* b) { return *b < *a; });

  for (auto* profiler : s_all_profilers)
    profiler->Print(buffer);
  s_lazy_result = buffer.str();
  return s_lazy_result;
}

void Profiler::Start()
{
  if (!m_depth++)
  {
    m_time = Common::Timer::NowUs();
  }
}

void Profiler::Stop()
{
  if (!--m_depth)
  {
    u64 end = Common::Timer::NowUs();

    u64 diff = end - m_time;

    m_usecs += diff;
    m_usecs_min = std::min(m_usecs_min, diff);
    m_usecs_max = std::max(m_usecs_max, diff);
    m_usecs_quad += diff * diff;
    m_calls++;
  }
}

void Profiler::Print(std::ostream& os)
{
  double avg = 0;
  double stdev = 0;
  double time_rel = 0;
  if (m_calls)
  {
    avg = double(m_usecs) / m_calls;
    stdev = std::sqrt(double(m_usecs_quad) / m_calls - avg * avg);
  }
  else
  {
    m_usecs_min = 0;
  }
  if (s_usecs_frame)
  {
    time_rel = double(m_usecs) * 100 / s_usecs_frame;
  }

  fmt::print(os, "{:<{}s} {:>{}d} {:>{}d} {:>{}f} {:>{}d} {:>{}.2f} {:>{}.2f} {:>{}d}\n",  //
             m_name, s_max_length,                                                         //
             m_calls, PROFILER_FIELD_LENGTH,                                               //
             m_usecs, PROFILER_FIELD_LENGTH,                                               //
             time_rel, PROFILER_FIELD_LENGTH_FP,                                           //
             m_usecs_min, PROFILER_FIELD_LENGTH,                                           //
             avg, PROFILER_FIELD_LENGTH_FP,                                                //
             stdev, PROFILER_FIELD_LENGTH_FP,                                              //
             m_usecs_max, PROFILER_FIELD_LENGTH);                                          //

  m_usecs = 0;
  m_usecs_min = UINT64_MAX;
  m_usecs_max = 0;
  m_usecs_quad = 0;
  m_calls = 0;
}
}  // namespace Common
