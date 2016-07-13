// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/CommonTypes.h"

#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
class StateTracker;

class FrameTimer
{
public:
  FrameTimer();
  ~FrameTimer();

  bool Initialize(bool enabled);
  void Reset();

  bool IsEnabled() const { return m_enabled; }
  void SetEnabled(bool enable);

  // Call after submitting the command buffer.
  void BeginFrame();

  // Call before submitting the command buffer.
  void EndFrame();

  double GetLastCPUTime() const { return m_last_cpu_time; }
  double GetLastGPUTime() const { return m_last_gpu_time; }
  double GetMinCPUTime() const { return m_min_cpu_time; }
  double GetMinGPUTime() const { return m_min_gpu_time; }
  double GetAverageCPUTime() const { return m_avg_cpu_time; }
  double GetAverageGPUTime() const { return m_avg_gpu_time; }
  double GetMaxCPUTime() const { return m_max_cpu_time; }
  double GetMaxGPUTime() const { return m_max_gpu_time; }
private:
  bool CreateTimestampQueryPool();
  void UpdateCPUTime();
  void StartGPUTimer();
  void UpdateGPUTime();
  void UpdateHistory();

  bool m_enabled = false;

  u64 m_frame_start_cpu_time = 0;

  // start + end for each frame
  static const u32 TIMESTAMP_QUERIES_PER_FRAME = 2;
  static const u32 TIMESTAMP_QUERY_START_TIME = 0;
  static const u32 TIMESTAMP_QUERY_END_TIME = 1;
  static const u32 TIMESTAMP_QUERY_COUNT = TIMESTAMP_QUERIES_PER_FRAME * 2;
  VkQueryPool m_timestamp_query_pool = VK_NULL_HANDLE;
  u64 m_timestamp_value_mask = 0;
  u32 m_previous_query_index = 0;
  u32 m_current_query_index = 0;
  bool m_has_previous_frame_query = false;
  bool m_has_current_frame_query = false;
  bool m_supports_timestamps = false;

  double m_last_cpu_time = 0.0;
  double m_last_gpu_time = 0.0;

  static const size_t HISTORY_COUNT = 100;
  std::array<double, HISTORY_COUNT> m_cpu_time_history = {};
  std::array<double, HISTORY_COUNT> m_gpu_time_history = {};
  size_t m_history_position = 0;

  double m_min_cpu_time = 0.0;
  double m_max_cpu_time = 0.0;
  double m_avg_cpu_time = 0.0;
  double m_min_gpu_time = 0.0;
  double m_max_gpu_time = 0.0;
  double m_avg_gpu_time = 0.0;
};

}  // namespace Vulkan
