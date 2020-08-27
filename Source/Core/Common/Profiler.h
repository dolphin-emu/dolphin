// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

  void Start();
  void Stop();
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

  std::string m_name;
  u64 m_usecs;
  u64 m_usecs_min;
  u64 m_usecs_max;
  u64 m_usecs_quad;
  u64 m_calls;
  u64 m_time;
  int m_depth;
};

class ProfilerExecuter
{
public:
  ProfilerExecuter(Profiler* _p) : m_p(_p) { m_p->Start(); }
  ~ProfilerExecuter() { m_p->Stop(); }

private:
  Profiler* m_p;
};
};  // namespace Common

// Warning: This profiler isn't thread safe. Only profile functions which doesn't run simultaneously
#define PROFILE(name)                                                                              \
  static Common::Profiler prof_gen(name);                                                          \
  Common::ProfilerExecuter prof_e(&prof_gen);
