// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>

#include "Common/CommonTypes.h"

// Helper to calculate the ratio between (audio) input and output, in seconds.
// Ticks represents the num of samples, while updates represent samples pushes.
struct AudioSpeedCounter
{
  AudioSpeedCounter(double average_time, double ticks_per_sec, double ticks_per_upd);

  // Start does not need to be called. Can be used as re-start
  void Start(bool simulate_full_speed = false);
  // if elapsed_ticks changes, the m_last_deltas array is adjusted to keep the same average speed
  void Update(double elapsed_ticks);
  // Current (last calculated). Thread safe
  double GetLastSpeed(bool& in_out_predict, bool simulate_full_speed = false) const;
  // Use max_average_time to get a more "precise" estimation. Not thread safe
  double GetAverageSpeed(bool predict = false, bool simulate_full_speed = false,
                         double max_average_time = -1.0) const;
  // Thread safe
  double GetCachedAverageSpeed(bool alternative_speed = false, bool predict = false,
                               bool simulate_full_speed = false) const;
  void CacheAverageSpeed(bool alternative_speed = false, double max_average_time = -1.0);
  double GetAverageTime() const { return m_average_time; }
  double GetTargetDelta() const { return m_target_delta; }
  void SetPaused(bool paused);
  bool IsPaused() { return m_is_paused; }
  // Sets the length time to count the speed for
  void SetAverageTime(double average_time);
  void SetTicksPerSecond(double ticks_per_sec);

private:
  // Time passed between every update
  std::deque<double> m_last_deltas;
  std::atomic<u64> m_last_time;
  std::atomic<double> m_cached_average{0.0};
  std::atomic<double> m_alternative_cached_average{0.0};
  std::atomic<u32> m_cached_average_num{0};
  std::atomic<u32> m_alternative_cached_average_num{0};
  std::atomic<double> m_cached_last_delta{-1.0};
  // The max time we keep deltas for
  double m_average_time;
  double m_ticks_per_upd;
  double m_ticks_per_sec;
  // The expected time elapsed between two updates
  double m_target_delta = 1.0;
  u64 m_last_paused_time = 0;
  std::atomic<bool> m_is_paused{false};

  void OnSettingsChanged();

  // Us
  u64 GetTime() const;
  // Seconds
  double GetTimeDelta(u64 old_time) const;
  double GetTimeDeltaAndUpdateOldTime(std::atomic<u64>& old_time) const;

  static constexpr double TIME_CONVERSION = 1000000.0;
};
