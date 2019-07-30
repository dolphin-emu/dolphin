// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/PerfQuery.h"

#include <algorithm>
#include <cstring>
#include <functional>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/VideoCommon.h"

namespace Vulkan
{
PerfQuery::PerfQuery() = default;

PerfQuery::~PerfQuery()
{
  if (m_query_pool != VK_NULL_HANDLE)
    vkDestroyQueryPool(g_vulkan_context->GetDevice(), m_query_pool, nullptr);
}

bool PerfQuery::Initialize()
{
  if (!CreateQueryPool())
  {
    PanicAlert("Failed to create query pool");
    return false;
  }

  return true;
}

void PerfQuery::EnableQuery(PerfQueryGroup type)
{
  // Block if there are no free slots.
  // Otherwise, try to keep half of them available.
  if (m_query_count > m_query_buffer.size() / 2)
    PartialFlush(m_query_count == PERF_QUERY_BUFFER_SIZE);

  // Ensure command buffer is ready to go before beginning the query, that way we don't submit
  // a buffer with open queries.
  StateTracker::GetInstance()->Bind();

  if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
  {
    ActiveQuery& entry = m_query_buffer[m_query_next_pos];
    DEBUG_ASSERT(!entry.has_value);
    entry.has_value = true;

    // Use precise queries if supported, otherwise boolean (which will be incorrect).
    VkQueryControlFlags flags =
        g_vulkan_context->SupportsPreciseOcclusionQueries() ? VK_QUERY_CONTROL_PRECISE_BIT : 0;

    // Ensure the query starts within a render pass.
    StateTracker::GetInstance()->BeginRenderPass();
    vkCmdBeginQuery(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_query_pool, m_query_next_pos,
                    flags);
  }
}

void PerfQuery::DisableQuery(PerfQueryGroup type)
{
  if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
  {
    vkCmdEndQuery(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_query_pool, m_query_next_pos);
    m_query_next_pos = (m_query_next_pos + 1) % PERF_QUERY_BUFFER_SIZE;
    m_query_count++;
  }
}

void PerfQuery::ResetQuery()
{
  m_query_count = 0;
  m_query_readback_pos = 0;
  m_query_next_pos = 0;
  std::fill(std::begin(m_results), std::end(m_results), 0);

  // Reset entire query pool, ensuring all queries are ready to write to.
  StateTracker::GetInstance()->EndRenderPass();
  vkCmdResetQueryPool(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_query_pool, 0,
                      PERF_QUERY_BUFFER_SIZE);

  std::memset(m_query_buffer.data(), 0, sizeof(ActiveQuery) * m_query_buffer.size());
}

u32 PerfQuery::GetQueryResult(PerfQueryType type)
{
  u32 result = 0;
  if (type == PQ_ZCOMP_INPUT_ZCOMPLOC || type == PQ_ZCOMP_OUTPUT_ZCOMPLOC)
    result = m_results[PQG_ZCOMP_ZCOMPLOC];
  else if (type == PQ_ZCOMP_INPUT || type == PQ_ZCOMP_OUTPUT)
    result = m_results[PQG_ZCOMP];
  else if (type == PQ_BLEND_INPUT)
    result = m_results[PQG_ZCOMP] + m_results[PQG_ZCOMP_ZCOMPLOC];
  else if (type == PQ_EFB_COPY_CLOCKS)
    result = m_results[PQG_EFB_COPY_CLOCKS];

  return result / 4;
}

void PerfQuery::FlushResults()
{
  while (!IsFlushed())
    PartialFlush(true);
}

bool PerfQuery::IsFlushed() const
{
  return m_query_count == 0;
}

bool PerfQuery::CreateQueryPool()
{
  VkQueryPoolCreateInfo info = {
      VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,  // VkStructureType                  sType
      nullptr,                                   // const void*                      pNext
      0,                                         // VkQueryPoolCreateFlags           flags
      VK_QUERY_TYPE_OCCLUSION,                   // VkQueryType                      queryType
      PERF_QUERY_BUFFER_SIZE,                    // uint32_t                         queryCount
      0  // VkQueryPipelineStatisticFlags    pipelineStatistics;
  };

  VkResult res = vkCreateQueryPool(g_vulkan_context->GetDevice(), &info, nullptr, &m_query_pool);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateQueryPool failed: ");
    return false;
  }

  return true;
}

void PerfQuery::ReadbackQueries()
{
  const u64 completed_fence_counter = g_command_buffer_mgr->GetCompletedFenceCounter();

  // Need to save these since ProcessResults will modify them.
  const u32 outstanding_queries = m_query_count;
  u32 readback_count = 0;
  for (u32 i = 0; i < outstanding_queries; i++)
  {
    u32 index = (m_query_readback_pos + readback_count) % PERF_QUERY_BUFFER_SIZE;
    const ActiveQuery& entry = m_query_buffer[index];
    if (entry.fence_counter > completed_fence_counter)
      break;

    // If this wrapped around, we need to flush the entries before the end of the buffer.
    if (index < m_query_readback_pos)
    {
      ReadbackQueries(readback_count);
      DEBUG_ASSERT(m_query_readback_pos == 0);
      readback_count = 0;
    }

    readback_count++;
  }

  if (readback_count > 0)
    ReadbackQueries(readback_count);
}

void PerfQuery::ReadbackQueries(u32 query_count)
{
  // Should be at maximum query_count queries pending.
  ASSERT(query_count <= m_query_count &&
         (m_query_readback_pos + query_count) <= PERF_QUERY_BUFFER_SIZE);

  // Read back from the GPU.
  VkResult res =
      vkGetQueryPoolResults(g_vulkan_context->GetDevice(), m_query_pool, m_query_readback_pos,
                            query_count, query_count * sizeof(PerfQueryDataType),
                            m_query_result_buffer.data(), sizeof(PerfQueryDataType), 0);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkGetQueryPoolResults failed: ");

  // Remove pending queries.
  for (u32 i = 0; i < query_count; i++)
  {
    u32 index = (m_query_readback_pos + i) % PERF_QUERY_BUFFER_SIZE;
    ActiveQuery& entry = m_query_buffer[index];

    // Should have a fence associated with it (waiting for a result).
    DEBUG_ASSERT(entry.fence_counter != 0);
    entry.fence_counter = 0;
    entry.has_value = false;

    // NOTE: Reported pixel metrics should be referenced to native resolution
    m_results[entry.query_type] +=
        static_cast<u32>(static_cast<u64>(m_query_result_buffer[i]) * EFB_WIDTH /
                         g_renderer->GetTargetWidth() * EFB_HEIGHT / g_renderer->GetTargetHeight());
  }

  m_query_readback_pos = (m_query_readback_pos + query_count) % PERF_QUERY_BUFFER_SIZE;
  m_query_count -= query_count;
}

void PerfQuery::PartialFlush(bool blocking)
{
  // Submit a command buffer in the background if the front query is not bound to one.
  if (blocking || m_query_buffer[m_query_readback_pos].fence_counter ==
                      g_command_buffer_mgr->GetCurrentFenceCounter())
  {
    Renderer::GetInstance()->ExecuteCommandBuffer(true, blocking);
  }

  ReadbackQueries();
}
}  // namespace Vulkan
