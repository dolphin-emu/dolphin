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
#include "VideoBackends/Vulkan/StagingBuffer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
PerfQuery::PerfQuery()
{
}

PerfQuery::~PerfQuery()
{
  g_command_buffer_mgr->RemoveFencePointCallback(this);

  if (m_query_pool != VK_NULL_HANDLE)
    vkDestroyQueryPool(g_vulkan_context->GetDevice(), m_query_pool, nullptr);
}

Vulkan::PerfQuery* PerfQuery::GetInstance()
{
  return static_cast<PerfQuery*>(g_perf_query.get());
}

bool PerfQuery::Initialize()
{
  if (!CreateQueryPool())
  {
    PanicAlert("Failed to create query pool");
    return false;
  }

  if (!CreateReadbackBuffer())
  {
    PanicAlert("Failed to create readback buffer");
    return false;
  }

  g_command_buffer_mgr->AddFencePointCallback(
      this, std::bind(&PerfQuery::OnCommandBufferQueued, this, std::placeholders::_1,
                      std::placeholders::_2),
      std::bind(&PerfQuery::OnCommandBufferExecuted, this, std::placeholders::_1));

  return true;
}

void PerfQuery::EnableQuery(PerfQueryGroup type)
{
  // Have we used half of the query buffer already?
  if (m_query_count > m_query_buffer.size() / 2)
    NonBlockingPartialFlush();

  // Block if there are no free slots.
  if (m_query_count == PERF_QUERY_BUFFER_SIZE)
  {
    // ERROR_LOG(VIDEO, "Flushed query buffer early!");
    BlockingPartialFlush();
  }

  if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
  {
    u32 index = (m_query_read_pos + m_query_count) % PERF_QUERY_BUFFER_SIZE;
    ActiveQuery& entry = m_query_buffer[index];
    _assert_(!entry.active && !entry.available);
    entry.active = true;
    m_query_count++;

    DEBUG_LOG(VIDEO, "start query %u", index);

    // Use precise queries if supported, otherwise boolean (which will be incorrect).
    VkQueryControlFlags flags = 0;
    if (g_vulkan_context->SupportsPreciseOcclusionQueries())
      flags = VK_QUERY_CONTROL_PRECISE_BIT;

    // Ensure the query starts within a render pass.
    // TODO: Is this needed?
    StateTracker::GetInstance()->BeginRenderPass();
    vkCmdBeginQuery(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_query_pool, index, flags);

    // Prevent background command buffer submission while the query is active.
    StateTracker::GetInstance()->SetBackgroundCommandBufferExecution(false);
  }
}

void PerfQuery::DisableQuery(PerfQueryGroup type)
{
  if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
  {
    // DisableQuery should be called for each EnableQuery, so subtract one to get the previous one.
    u32 index = (m_query_read_pos + m_query_count - 1) % PERF_QUERY_BUFFER_SIZE;
    vkCmdEndQuery(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_query_pool, index);
    StateTracker::GetInstance()->SetBackgroundCommandBufferExecution(true);
    DEBUG_LOG(VIDEO, "end query %u", index);
  }
}

void PerfQuery::ResetQuery()
{
  m_query_count = 0;
  m_query_read_pos = 0;
  std::fill_n(m_results, ArraySize(m_results), 0);

  // Reset entire query pool, ensuring all queries are ready to write to.
  StateTracker::GetInstance()->EndRenderPass();
  vkCmdResetQueryPool(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_query_pool, 0,
                      PERF_QUERY_BUFFER_SIZE);

  for (auto& entry : m_query_buffer)
  {
    entry.pending_fence = VK_NULL_HANDLE;
    entry.available = false;
    entry.active = false;
  }
}

u32 PerfQuery::GetQueryResult(PerfQueryType type)
{
  u32 result = 0;

  if (type == PQ_ZCOMP_INPUT_ZCOMPLOC || type == PQ_ZCOMP_OUTPUT_ZCOMPLOC)
  {
    result = m_results[PQG_ZCOMP_ZCOMPLOC];
  }
  else if (type == PQ_ZCOMP_INPUT || type == PQ_ZCOMP_OUTPUT)
  {
    result = m_results[PQG_ZCOMP];
  }
  else if (type == PQ_BLEND_INPUT)
  {
    result = m_results[PQG_ZCOMP] + m_results[PQG_ZCOMP_ZCOMPLOC];
  }
  else if (type == PQ_EFB_COPY_CLOCKS)
  {
    result = m_results[PQG_EFB_COPY_CLOCKS];
  }

  return result / 4;
}

void PerfQuery::FlushResults()
{
  while (!IsFlushed())
    BlockingPartialFlush();
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

bool PerfQuery::CreateReadbackBuffer()
{
  m_readback_buffer = StagingBuffer::Create(STAGING_BUFFER_TYPE_READBACK,
                                            PERF_QUERY_BUFFER_SIZE * sizeof(PerfQueryDataType),
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  // Leave the buffer persistently mapped, we invalidate it when we need to read.
  if (!m_readback_buffer || !m_readback_buffer->Map())
    return false;

  return true;
}

void PerfQuery::QueueCopyQueryResults(VkCommandBuffer command_buffer, VkFence fence,
                                      u32 start_index, u32 query_count)
{
  DEBUG_LOG(VIDEO, "queue copy of queries %u-%u", start_index, start_index + query_count - 1);

  // Transition buffer for GPU write
  // TODO: Is this needed?
  m_readback_buffer->PrepareForGPUWrite(command_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Copy from queries -> buffer
  vkCmdCopyQueryPoolResults(command_buffer, m_query_pool, start_index, query_count,
                            m_readback_buffer->GetBuffer(), start_index * sizeof(PerfQueryDataType),
                            sizeof(PerfQueryDataType), VK_QUERY_RESULT_WAIT_BIT);

  // Prepare for host readback
  m_readback_buffer->FlushGPUCache(command_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Reset queries so they're ready to use again
  vkCmdResetQueryPool(command_buffer, m_query_pool, start_index, query_count);

  // Flag all queries as available, but with a fence that has to be completed first
  for (u32 i = 0; i < query_count; i++)
  {
    u32 index = start_index + i;
    ActiveQuery& entry = m_query_buffer[index];
    entry.pending_fence = fence;
    entry.available = true;
    entry.active = false;
  }
}

void PerfQuery::OnCommandBufferQueued(VkCommandBuffer command_buffer, VkFence fence)
{
  // Flag all pending queries that aren't available as available after execution.
  u32 copy_start_index = 0;
  u32 copy_count = 0;
  for (u32 i = 0; i < m_query_count; i++)
  {
    u32 index = (m_query_read_pos + i) % PERF_QUERY_BUFFER_SIZE;
    ActiveQuery& entry = m_query_buffer[index];

    // Skip already-copied queries (will happen if a flush hasn't occurred and
    // a command buffer hasn't finished executing).
    if (entry.available)
    {
      // These should be grouped together, and at the start.
      _assert_(copy_count == 0);
      continue;
    }

    // If this wrapped around, we need to flush the entries before the end of the buffer.
    _assert_(entry.active);
    if (index < copy_start_index)
    {
      QueueCopyQueryResults(command_buffer, fence, copy_start_index, copy_count);
      copy_start_index = index;
      copy_count = 0;
    }
    else if (copy_count == 0)
    {
      copy_start_index = index;
    }
    copy_count++;
  }

  if (copy_count > 0)
    QueueCopyQueryResults(command_buffer, fence, copy_start_index, copy_count);
}

void PerfQuery::OnCommandBufferExecuted(VkFence fence)
{
  // Need to save these since ProcessResults will modify them.
  u32 query_read_pos = m_query_read_pos;
  u32 query_count = m_query_count;

  // Flush as many queries as are bound to this fence.
  u32 flush_start_index = 0;
  u32 flush_count = 0;
  for (u32 i = 0; i < query_count; i++)
  {
    u32 index = (query_read_pos + i) % PERF_QUERY_BUFFER_SIZE;
    if (m_query_buffer[index].pending_fence != fence)
    {
      // These should be grouped together, at the end.
      break;
    }

    // If this wrapped around, we need to flush the entries before the end of the buffer.
    if (index < flush_start_index)
    {
      ProcessResults(flush_start_index, flush_count);
      flush_start_index = index;
      flush_count = 0;
    }
    else if (flush_count == 0)
    {
      flush_start_index = index;
    }
    flush_count++;
  }

  if (flush_count > 0)
    ProcessResults(flush_start_index, flush_count);
}

void PerfQuery::ProcessResults(u32 start_index, u32 query_count)
{
  // Invalidate CPU caches before reading back.
  m_readback_buffer->InvalidateCPUCache(start_index * sizeof(PerfQueryDataType),
                                        query_count * sizeof(PerfQueryDataType));

  // Should be at maximum query_count queries pending.
  _assert_(query_count <= m_query_count);
  DEBUG_LOG(VIDEO, "process queries %u-%u", start_index, start_index + query_count - 1);

  // Remove pending queries.
  for (u32 i = 0; i < query_count; i++)
  {
    u32 index = (m_query_read_pos + i) % PERF_QUERY_BUFFER_SIZE;
    ActiveQuery& entry = m_query_buffer[index];

    // Should have a fence associated with it (waiting for a result).
    _assert_(entry.pending_fence != VK_NULL_HANDLE);
    entry.pending_fence = VK_NULL_HANDLE;
    entry.available = false;
    entry.active = false;

    // Grab result from readback buffer, it will already have been invalidated.
    u32 result;
    m_readback_buffer->Read(index * sizeof(PerfQueryDataType), &result, sizeof(result), false);
    DEBUG_LOG(VIDEO, "  query result %u", result);

    // NOTE: Reported pixel metrics should be referenced to native resolution
    m_results[entry.query_type] +=
        static_cast<u32>(static_cast<u64>(result) * EFB_WIDTH / g_renderer->GetTargetWidth() *
                         EFB_HEIGHT / g_renderer->GetTargetHeight());
  }

  m_query_read_pos = (m_query_read_pos + query_count) % PERF_QUERY_BUFFER_SIZE;
  m_query_count -= query_count;
}

void PerfQuery::NonBlockingPartialFlush()
{
  if (IsFlushed())
    return;

  // Submit a command buffer in the background if the front query is not bound to one.
  // Ideally this will complete before the buffer fills.
  if (m_query_buffer[m_query_read_pos].pending_fence == VK_NULL_HANDLE)
    Util::ExecuteCurrentCommandsAndRestoreState(true, false);
}

void PerfQuery::BlockingPartialFlush()
{
  if (IsFlushed())
    return;

  // If the first pending query is needing command buffer execution, do that.
  ActiveQuery& entry = m_query_buffer[m_query_read_pos];
  if (entry.pending_fence == VK_NULL_HANDLE)
  {
    // This will callback OnCommandBufferQueued which will set the fence on the entry.
    // We wait for completion, which will also call OnCommandBufferExecuted, and clear the fence.
    Util::ExecuteCurrentCommandsAndRestoreState(false, true);
  }
  else
  {
    // The command buffer has been submitted, but is awaiting completion.
    // Wait for the fence to complete, which will call OnCommandBufferExecuted.
    g_command_buffer_mgr->WaitForFence(entry.pending_fence);
  }
}
}
