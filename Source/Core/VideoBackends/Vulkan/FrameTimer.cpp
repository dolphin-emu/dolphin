// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstdint>

#include "Common/Assert.h"
#include "Common/Timer.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FrameTimer.h"
#include "VideoBackends/Vulkan/ObjectCache.h"

namespace Vulkan
{
FrameTimer::FrameTimer()
{
}

FrameTimer::~FrameTimer()
{
  if (m_timestamp_query_pool != VK_NULL_HANDLE)
    vkDestroyQueryPool(g_object_cache->GetDevice(), m_timestamp_query_pool, nullptr);
}

bool FrameTimer::Initialize(bool enabled)
{
  m_enabled = enabled;
  m_frame_start_cpu_time = Common::Timer::GetTimeUs();

  if (!CreateTimestampQueryPool())
    return false;

  return true;
}

void FrameTimer::Reset()
{
  // Don't bother with resetting the queries, they'll be reset before they're next used.
  m_has_previous_frame_query = false;
  m_has_current_frame_query = false;
  m_current_query_index = 0;
  m_previous_query_index = 0;
  m_frame_start_cpu_time = Common::Timer::GetTimeUs();
}

void FrameTimer::SetEnabled(bool enable)
{
  if (m_enabled == enable)
    return;

  m_enabled = enable;
  Reset();
}

void FrameTimer::BeginFrame()
{
  if (!m_enabled)
    return;

  StartGPUTimer();
}

void FrameTimer::EndFrame()
{
  if (!m_enabled)
    return;

  // Record the ending CPU time first before waiting on the GPU.
  UpdateCPUTime();
  UpdateGPUTime();
  UpdateHistory();
}

bool FrameTimer::CreateTimestampQueryPool()
{
  if (g_command_buffer_mgr->GetGraphicsQueueFamilyProperties().timestampValidBits == 0)
  {
    m_supports_timestamps = false;
    return true;
  }

  VkQueryPoolCreateInfo info = {
      VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,  // VkStructureType                  sType
      nullptr,                                   // const void*                      pNext
      0,                                         // VkQueryPoolCreateFlags           flags
      VK_QUERY_TYPE_TIMESTAMP,                   // VkQueryType                      queryType
      TIMESTAMP_QUERY_COUNT,                     // uint32_t                         queryCount
      0  // VkQueryPipelineStatisticFlags    pipelineStatistics;
  };
  VkResult res =
      vkCreateQueryPool(g_object_cache->GetDevice(), &info, nullptr, &m_timestamp_query_pool);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateQueryPool failed: ");
    return false;
  }

  u32 valid_bits = g_command_buffer_mgr->GetGraphicsQueueFamilyProperties().timestampValidBits;
  if (valid_bits != 64)
    m_timestamp_value_mask = (UINT64_C(1) << (valid_bits + 1)) - 1;
  else
    m_timestamp_value_mask = UINT64_MAX;

  m_supports_timestamps = true;
  return true;
}

void FrameTimer::UpdateCPUTime()
{
  u64 current_time = Common::Timer::GetTimeUs();
  u64 time_diff = current_time - m_frame_start_cpu_time;
  m_frame_start_cpu_time = current_time;

  // Nanoseconds -> seconds
  m_last_cpu_time = static_cast<double>(time_diff) / 1000000.0;
}

void FrameTimer::StartGPUTimer()
{
  if (!m_supports_timestamps)
    return;

  m_has_current_frame_query = true;

  // Reset the next queries in the buffer to unavailable.
  vkCmdResetQueryPool(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_timestamp_query_pool,
                      m_current_query_index, TIMESTAMP_QUERIES_PER_FRAME);

  // Record the starting time of the next frame.
  vkCmdWriteTimestamp(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestamp_query_pool,
                      m_current_query_index + TIMESTAMP_QUERY_START_TIME);
}

void FrameTimer::UpdateGPUTime()
{
  if (!m_supports_timestamps)
    return;

  // Write the timestamp for the end of this frame.
  // NOTE: This won't include presentation time.
  if (m_has_current_frame_query)
  {
    vkCmdWriteTimestamp(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestamp_query_pool,
                        m_current_query_index + TIMESTAMP_QUERY_END_TIME);
  }

  // Resolve the query for the last frame
  std::array<u64, TIMESTAMP_QUERIES_PER_FRAME> query_results = {};
  if (m_has_previous_frame_query)
  {
    // TODO: When blocking readbacks are used, this fails (At least on AMD). Investigate.
    VkResult res = vkGetQueryPoolResults(
        g_object_cache->GetDevice(), m_timestamp_query_pool, m_previous_query_index,
        static_cast<uint32_t>(query_results.size()), sizeof(u64) * query_results.size(),
        query_results.data(), sizeof(u64), VK_QUERY_RESULT_64_BIT);

    // Valid results?
    if (res != VK_SUCCESS)
      LOG_VULKAN_ERROR(res, "vkGetQueryPoolResults failed: ");

    // Mask away invalid bits.
    for (u64& value : query_results)
      value &= m_timestamp_value_mask;
  }

  // Store current state for the next frame.
  m_previous_query_index = m_current_query_index;
  m_has_previous_frame_query = m_has_current_frame_query;

  // Increment query position
  m_current_query_index += TIMESTAMP_QUERIES_PER_FRAME;
  m_current_query_index %= TIMESTAMP_QUERY_COUNT;

  // Update time. Values are in nanoseconds, so convert to seconds.
  m_last_gpu_time = static_cast<double>(query_results[TIMESTAMP_QUERY_END_TIME] -
                                        query_results[TIMESTAMP_QUERY_START_TIME]) *
                    static_cast<double>(g_object_cache->GetDeviceLimits().timestampPeriod) /
                    1000000000.0;
}

void FrameTimer::UpdateHistory()
{
  m_cpu_time_history[m_history_position] = m_last_cpu_time;
  m_gpu_time_history[m_history_position] = m_last_gpu_time;
  m_history_position = (m_history_position + 1) % HISTORY_COUNT;

  m_min_cpu_time = m_last_cpu_time;
  m_max_cpu_time = m_last_cpu_time;
  m_avg_cpu_time = 0;
  m_min_gpu_time = m_last_gpu_time;
  m_max_gpu_time = m_last_gpu_time;
  m_avg_gpu_time = 0;
  for (size_t i = 0; i < HISTORY_COUNT; i++)
  {
    m_min_cpu_time = std::min(m_min_cpu_time, m_cpu_time_history[i]);
    m_max_cpu_time = std::max(m_max_cpu_time, m_cpu_time_history[i]);
    m_avg_cpu_time += m_cpu_time_history[i];
    m_min_gpu_time = std::min(m_min_gpu_time, m_gpu_time_history[i]);
    m_max_gpu_time = std::max(m_max_gpu_time, m_gpu_time_history[i]);
    m_avg_gpu_time += m_gpu_time_history[i];
  }
  m_avg_cpu_time /= HISTORY_COUNT;
  m_avg_gpu_time /= HISTORY_COUNT;
}

}  // namespace Vulkan
