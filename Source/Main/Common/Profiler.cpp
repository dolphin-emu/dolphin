// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <ios>
#include <sstream>

#include "Common/Profiler.h"
#include "Common/Timer.h"

namespace Common
{
static const u32 PROFILER_FIELD_LENGTH = 8;
static const u32 PROFILER_FIELD_LENGTH_FP = PROFILER_FIELD_LENGTH + 3;
static const int PROFILER_LAZY_DELAY = 60;  // in frames

std::list<Profiler*> Profiler::s_all_profilers;
std::mutex Profiler::s_mutex;
u32 Profiler::s_max_length = 0;
u64 Profiler::s_frame_time;
u64 Profiler::s_usecs_frame;

std::string Profiler::s_lazy_result = "";
int Profiler::s_lazy_delay = 0;

Profiler::Profiler(const std::string& name)
    : m_name(name), m_usecs(0), m_usecs_min(UINT64_MAX), m_usecs_max(0), m_usecs_quad(0),
      m_calls(0), m_depth(0)
{
  m_time = Common::Timer::GetTimeUs();
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

  u64 end = Common::Timer::GetTimeUs();
  s_usecs_frame = end - s_frame_time;
  s_frame_time = end;

  std::ostringstream buffer;
  buffer << std::setw(s_max_length) << std::left << ""
         << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH) << std::right << "calls"
         << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH) << std::right << "sum"
         << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH_FP) << std::right << "rel"
         << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH) << std::right << "min"
         << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH_FP) << std::right << "avg"
         << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH_FP) << std::right << "stdev"
         << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH) << std::right << "max"
         << " ";
  buffer << "/ usec" << std::endl;

  s_all_profilers.sort([](Profiler* a, Profiler* b) { return *b < *a; });

  for (auto profiler : s_all_profilers)
  {
    buffer << profiler->Read() << std::endl;
  }
  s_lazy_result = buffer.str();
  return s_lazy_result;
}

void Profiler::Start()
{
  if (!m_depth++)
  {
    m_time = Common::Timer::GetTimeUs();
  }
}

void Profiler::Stop()
{
  if (!--m_depth)
  {
    u64 end = Common::Timer::GetTimeUs();

    u64 diff = end - m_time;

    m_usecs += diff;
    m_usecs_min = std::min(m_usecs_min, diff);
    m_usecs_max = std::max(m_usecs_max, diff);
    m_usecs_quad += diff * diff;
    m_calls++;
  }
}

std::string Profiler::Read()
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

  std::ostringstream buffer;

  buffer << std::setw(s_max_length) << std::left << m_name << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH) << std::right << m_calls << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH) << std::right << m_usecs << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH_FP) << std::right << time_rel << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH) << std::right << m_usecs_min << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH_FP) << std::right << std::fixed << std::setprecision(2)
         << avg << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH_FP) << std::right << std::fixed << std::setprecision(2)
         << stdev << " ";
  buffer << std::setw(PROFILER_FIELD_LENGTH) << std::right << m_usecs_max;

  m_usecs = 0;
  m_usecs_min = UINT64_MAX;
  m_usecs_max = 0;
  m_usecs_quad = 0;
  m_calls = 0;

  return buffer.str();
}
}
