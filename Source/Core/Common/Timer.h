// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "Common/CommonTypes.h"

namespace Common
{
class Timer
{
public:
  static u64 NowUs();
  static u64 NowMs();

  void Start();
  // Start(), then decrement start time by the offset.
  // Effectively "resumes" a timer
  void StartWithOffset(u64 offset);
  void Stop();
  bool IsRunning() const { return m_running; }
  u64 ElapsedMs() const;

  static u64 GetLocalTimeSinceJan1970();

  // Returns biased system timestamp as double
  // It is very unlikely you want to use this in new code; ideally we can remove it completely.
  static double GetSystemTimeAsDouble();
  // Formats a timestamp from GetSystemTimeAsDouble() into a date and time string
  static std::string SystemTimeAsDoubleToString(double time);

  static void IncreaseResolution();
  static void RestoreResolution();

  // Arbitrarily chosen value (38 years) that is subtracted in GetSystemTimeAsDouble()
  // to increase sub-second precision of the resulting double timestamp
  static constexpr int DOUBLE_TIME_OFFSET = (38 * 365 * 24 * 60 * 60);

private:
  u64 m_start_ms{0};
  u64 m_end_ms{0};
  bool m_running{false};
};

}  // Namespace Common
