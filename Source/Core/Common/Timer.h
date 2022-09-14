// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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
  u64 ElapsedMs() const;

  // The rest of these functions probably belong somewhere else
  static u64 GetLocalTimeSinceJan1970();

  static void IncreaseResolution();
  static void RestoreResolution();

private:
  u64 m_start_ms{0};
  u64 m_end_ms{0};
  bool m_running{false};
};

}  // Namespace Common
