// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/AudioSpeedCounter.h"

#include "Common/Timer.h"

AudioSpeedCounter::AudioSpeedCounter(double average_time, double ticks_per_sec,
                                     double ticks_per_upd)
    : m_average_time(average_time), m_ticks_per_sec(ticks_per_sec), m_ticks_per_upd(ticks_per_upd)
{
  m_ticks_per_sec = std::max(m_ticks_per_sec, 1.0);
  m_ticks_per_upd = std::max(m_ticks_per_upd, 1.0);
  OnSettingsChanged();

  m_last_time = GetTime();
}

u64 AudioSpeedCounter::GetTime() const
{
  return Common::Timer::GetTimeUs();
}
double AudioSpeedCounter::GetTimeDelta(u64 old_time) const
{
  const u64 time = GetTime();
  const u64 delta = time - old_time;
  return double(delta / TIME_CONVERSION);
}
double AudioSpeedCounter::GetTimeDeltaAndUpdateOldTime(std::atomic<u64>& old_time) const
{
  const u64 time = GetTime();
  const u64 delta = time - old_time;
  old_time = time;
  return double(delta / TIME_CONVERSION);
}

void AudioSpeedCounter::OnSettingsChanged()
{
  const double prev_target_delta = m_target_delta;
  m_target_delta = m_ticks_per_upd / m_ticks_per_sec;
  // Keep the previous speed by adjusting the last deltas
  const double relative_change = m_target_delta / prev_target_delta;
  for (size_t i = 0; i < m_last_deltas.size(); ++i)
  {
    m_last_deltas[i] *= relative_change;
  }
}

void AudioSpeedCounter::SetAverageTime(double average_time)
{
  // Don't immediately delete older deltas, it's not necessary for now
  m_average_time = average_time;
}
void AudioSpeedCounter::SetTicksPerSecond(double ticks_per_sec)
{
  m_ticks_per_sec = std::max(ticks_per_sec, 1.0);
  OnSettingsChanged();
}

void AudioSpeedCounter::Start(bool simulate_full_speed)
{
  m_last_time = GetTime();

  m_last_deltas.clear();
  if (simulate_full_speed)
  {
    m_cached_last_delta = m_target_delta;
    const size_t size = std::max(size_t(m_average_time / m_target_delta), size_t(1));
    m_last_deltas.resize(size, m_target_delta);
  }
  else
  {
    m_cached_last_delta = -1.0;
  }
}
void AudioSpeedCounter::Update(double elapsed_ticks)
{
  if (elapsed_ticks != m_ticks_per_upd)
  {
    m_ticks_per_upd = std::max(elapsed_ticks, 1.0);
    OnSettingsChanged();
  }

  const double delta = GetTimeDeltaAndUpdateOldTime(m_last_time);
  // If this ended up being 0 it would be ignored
  m_cached_last_delta = delta;
  double total_delta = delta;

  // Remove the oldest time deltas if they are older than the max average time
  for (int i = (int)m_last_deltas.size() - 1; i >= 0; --i)
  {
    total_delta += m_last_deltas[i];
    if (total_delta > m_average_time)
    {
      i = (int)m_last_deltas.size() - 1 - i;
      m_last_deltas.erase(m_last_deltas.begin(), m_last_deltas.end() - i);
      break;
    }
  }

  m_last_deltas.push_back(delta);
}

double AudioSpeedCounter::GetLastSpeed(bool& in_out_predict, bool simulate_full_speed) const
{
  if (in_out_predict)
  {
    const double delta = GetTimeDelta(m_last_time);
    // If it's currently late for a new update
    if (delta > m_target_delta)
    {
      return m_target_delta / delta;
    }
    in_out_predict = false;
  }

  double cached_last_delta = m_cached_last_delta;
  if (cached_last_delta > 0.0)
  {
    return m_target_delta / cached_last_delta;
  }
  return simulate_full_speed ? 1.0 : 0.0;
}

double AudioSpeedCounter::GetAverageSpeed(bool predict, bool simulate_full_speed,
                                          double max_average_time) const
{
  double total_delta = 0.0;
  u32 num = 0;

  if (predict)
  {
    const double delta = GetTimeDelta(m_last_time);
    // If it's currently late for a new update
    if (delta > m_target_delta)
    {
      total_delta += delta;
      ++num;
    }
  }

  for (int i = (int)m_last_deltas.size() - 1; i >= 0; --i)
  {
    total_delta += m_last_deltas[i];
    ++num;

    // Accept the last one even if it was over the limit (we always keep one last delta in anyway)
    if (max_average_time >= 0.0 && total_delta > max_average_time)
    {
      break;
    }
  }

  if (num == 0)
  {
    return simulate_full_speed ? 1.0 : 0.0;
  }

  return double(m_target_delta / (total_delta / num));
}

double AudioSpeedCounter::GetCachedAverageSpeed(bool alternative_speed, bool predict,
                                                bool simulate_full_speed) const
{
  double total_delta = alternative_speed ? m_alternative_cached_average : m_cached_average;
  s32 num = alternative_speed ? m_alternative_cached_average_num : m_cached_average_num;

  if (predict)
  {
    const double delta = GetTimeDelta(m_last_time);
    // If it's currently late for a new update
    if (delta > m_target_delta)
    {
      // Take away some importance from the average, otherwise it would always have the same weight.
      // Theoretically we should remove the oldest deltas but we can't here. Keep at least 1 like
      // in GetAverageSpeed()
      s32 times_over = delta / m_target_delta;
      total_delta *= double(std::max(num - times_over, 1)) / std::max(num, 1);
      num = std::max(num - times_over, 1);

      total_delta += delta;
      ++num;
    }
  }
  if (num == 0)
  {
    return simulate_full_speed ? 1.0 : 0.0;
  }

  return double(m_target_delta / (total_delta / num));
}

void AudioSpeedCounter::CacheAverageSpeed(bool alternative_speed, double max_average_time)
{
  double total_delta = 0.0;
  u32 num = 0;
  for (int i = (int)m_last_deltas.size() - 1; i >= 0; --i)
  {
    total_delta += m_last_deltas[i];
    ++num;

    // Accept the last one even if it was over the limit (we always keep one last delta in anyway)
    if (max_average_time >= 0.0 && total_delta > max_average_time)
    {
      break;
    }
  }

  if (alternative_speed)
  {
    m_alternative_cached_average = total_delta;
    m_alternative_cached_average_num = num;
  }
  else
  {
    m_cached_average = total_delta;
    m_cached_average_num = num;
  }
}

void AudioSpeedCounter::SetPaused(bool paused)
{
  if (paused != m_is_paused)
  {
    m_is_paused = paused;
    if (m_is_paused)
    {
      m_last_paused_time = GetTime();
    }
    else
    {
      const s64 time = GetTime();
      const s64 delta = time - m_last_paused_time;
      m_last_time += delta;
    }
  }
}
