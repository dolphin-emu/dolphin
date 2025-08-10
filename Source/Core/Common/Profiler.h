// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>
#include <mutex>
#include <string>

#include "CommonTypes.h"

namespace Common
{
class Profiler
{
public:
  Profiler(const std::string& name);
  ~Profiler();

  static std::string ToString();

  void Start(u64* time, int* depth);
  void Stop(u64* time, int* depth);
  std::string Read();

  bool operator<(const Profiler& b) const;

private:
  static std::list<Profiler*> s_all_profilers;
  static std::mutex s_mutex;
  static u32 s_max_length;
  static u64 s_frame_time;
  static u64 s_usecs_frame;

  static std::string s_lazy_result;
  static int s_lazy_delay;

  std::mutex m_mutex;
  std::string m_name;
  u64 m_usecs;
  u64 m_usecs_min;
  u64 m_usecs_max;
  u64 m_usecs_quad;
  u64 m_calls;
};

class ProfilerExecuter
{
public:
  ProfilerExecuter(Profiler* profiler, u64* time, int* depth)
      : m_profiler(profiler), m_time(time), m_depth(depth)
  {
    m_profiler->Start(m_time, m_depth);
  }
  ~ProfilerExecuter() { m_profiler->Stop(m_time, m_depth); }

private:
  Profiler* m_profiler;
  u64* m_time;
  int* m_depth;
};
}  // namespace Common

#define PROFILE(name)                                                                              \
  static Common::Profiler prof_gen(name);                                                          \
  static thread_local u64 prof_time;                                                               \
  static thread_local int prof_depth;                                                              \
  Common::ProfilerExecuter prof_e(&prof_gen, &prof_time, &prof_depth);
