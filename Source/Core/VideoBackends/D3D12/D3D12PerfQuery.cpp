// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/D3D12PerfQuery.h"

#include <algorithm>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/D3D12Gfx.h"
#include "VideoBackends/D3D12/DX12Context.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{
PerfQuery::PerfQuery() = default;

PerfQuery::~PerfQuery() = default;

bool PerfQuery::Initialize()
{
  constexpr D3D12_QUERY_HEAP_DESC desc = {D3D12_QUERY_HEAP_TYPE_OCCLUSION, PERF_QUERY_BUFFER_SIZE};
  HRESULT hr = g_dx_context->GetDevice()->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_query_heap));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create query heap: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return false;

  constexpr D3D12_HEAP_PROPERTIES heap_properties = {D3D12_HEAP_TYPE_READBACK};
  constexpr D3D12_RESOURCE_DESC resource_desc = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                                 0,
                                                 PERF_QUERY_BUFFER_SIZE * sizeof(PerfQueryDataType),
                                                 1,
                                                 1,
                                                 1,
                                                 DXGI_FORMAT_UNKNOWN,
                                                 {1, 0},
                                                 D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                                 D3D12_RESOURCE_FLAG_NONE};
  hr = g_dx_context->GetDevice()->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr, IID_PPV_ARGS(&m_query_readback_buffer));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create query buffer: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return false;

  return true;
}

void PerfQuery::EnableQuery(PerfQueryGroup group)
{
  // Block if there are no free slots.
  // Otherwise, try to keep half of them available.
  const u32 query_count = m_query_count.load(std::memory_order_relaxed);
  if (query_count > m_query_buffer.size() / 2)
  {
    const bool do_resolve = m_unresolved_queries > m_query_buffer.size() / 2;
    const bool blocking = query_count == PERF_QUERY_BUFFER_SIZE;
    PartialFlush(do_resolve, blocking);
  }

  // Ensure all state is applied before beginning the query.
  // This is because we can't leave a query open when submitting a command list, and the draw
  // call itself may need to execute a command list if we run out of descriptors. Note that
  // this assumes that the caller has bound all required state prior to enabling the query.
  Gfx::GetInstance()->ApplyState();

  if (group == PQG_ZCOMP_ZCOMPLOC || group == PQG_ZCOMP)
  {
    ActiveQuery& entry = m_query_buffer[m_query_next_pos];
    ASSERT(!entry.has_value && !entry.resolved);
    entry.has_value = true;
    entry.query_group = group;

    g_dx_context->GetCommandList()->BeginQuery(m_query_heap.Get(), D3D12_QUERY_TYPE_OCCLUSION,
                                               m_query_next_pos);
  }
}

void PerfQuery::DisableQuery(PerfQueryGroup group)
{
  if (group == PQG_ZCOMP_ZCOMPLOC || group == PQG_ZCOMP)
  {
    g_dx_context->GetCommandList()->EndQuery(m_query_heap.Get(), D3D12_QUERY_TYPE_OCCLUSION,
                                             m_query_next_pos);
    m_query_next_pos = (m_query_next_pos + 1) % PERF_QUERY_BUFFER_SIZE;
    m_query_count.fetch_add(1, std::memory_order_relaxed);
    m_unresolved_queries++;
  }
}

void PerfQuery::ResetQuery()
{
  m_query_count.store(0, std::memory_order_relaxed);
  m_unresolved_queries = 0;
  m_query_resolve_pos = 0;
  m_query_readback_pos = 0;
  m_query_next_pos = 0;
  for (size_t i = 0; i < m_results.size(); ++i)
    m_results[i].store(0, std::memory_order_relaxed);
  for (auto& entry : m_query_buffer)
  {
    entry.fence_value = 0;
    entry.resolved = false;
    entry.has_value = false;
  }
}

u32 PerfQuery::GetQueryResult(PerfQueryType type)
{
  u32 result = 0;
  if (type == PQ_ZCOMP_INPUT_ZCOMPLOC || type == PQ_ZCOMP_OUTPUT_ZCOMPLOC)
  {
    result = m_results[PQG_ZCOMP_ZCOMPLOC].load(std::memory_order_relaxed);
  }
  else if (type == PQ_ZCOMP_INPUT || type == PQ_ZCOMP_OUTPUT)
  {
    result = m_results[PQG_ZCOMP].load(std::memory_order_relaxed);
  }
  else if (type == PQ_BLEND_INPUT)
  {
    result = m_results[PQG_ZCOMP].load(std::memory_order_relaxed) +
             m_results[PQG_ZCOMP_ZCOMPLOC].load(std::memory_order_relaxed);
  }
  else if (type == PQ_EFB_COPY_CLOCKS)
  {
    result = m_results[PQG_EFB_COPY_CLOCKS].load(std::memory_order_relaxed);
  }

  return result / 4;
}

void PerfQuery::FlushResults()
{
  while (!IsFlushed())
    PartialFlush(true, true);
}

bool PerfQuery::IsFlushed() const
{
  return m_query_count.load(std::memory_order_relaxed) == 0;
}

void PerfQuery::ResolveQueries()
{
  // Do we need to split the resolve as it's wrapping around?
  if ((m_query_resolve_pos + m_unresolved_queries) > PERF_QUERY_BUFFER_SIZE)
    ResolveQueries(PERF_QUERY_BUFFER_SIZE - m_query_resolve_pos);

  ResolveQueries(m_unresolved_queries);
}

void PerfQuery::ResolveQueries(u32 query_count)
{
  DEBUG_ASSERT(m_unresolved_queries >= query_count &&
               (m_query_resolve_pos + query_count) <= PERF_QUERY_BUFFER_SIZE);

  g_dx_context->GetCommandList()->ResolveQueryData(
      m_query_heap.Get(), D3D12_QUERY_TYPE_OCCLUSION, m_query_resolve_pos, query_count,
      m_query_readback_buffer.Get(), m_query_resolve_pos * sizeof(PerfQueryDataType));

  // Flag all queries as available, but with a fence that has to be completed first
  for (u32 i = 0; i < query_count; i++)
  {
    ActiveQuery& entry = m_query_buffer[m_query_resolve_pos + i];
    DEBUG_ASSERT(entry.has_value && !entry.resolved);
    entry.fence_value = g_dx_context->GetCurrentFenceValue();
    entry.resolved = true;
  }
  m_query_resolve_pos = (m_query_resolve_pos + query_count) % PERF_QUERY_BUFFER_SIZE;
  m_unresolved_queries -= query_count;
}

void PerfQuery::ReadbackQueries(bool blocking)
{
  u64 completed_fence_counter = g_dx_context->GetCompletedFenceValue();

  // Need to save these since ProcessResults will modify them.
  const u32 outstanding_queries = m_query_count.load(std::memory_order_relaxed);
  u32 readback_count = 0;
  for (u32 i = 0; i < outstanding_queries; i++)
  {
    u32 index = (m_query_readback_pos + readback_count) % PERF_QUERY_BUFFER_SIZE;
    const ActiveQuery& entry = m_query_buffer[index];
    if (!entry.resolved)
      break;

    if (entry.fence_value > completed_fence_counter)
    {
      // Query result isn't ready yet. Wait if blocking, otherwise we can't do any more yet.
      if (!blocking)
        break;

      ASSERT(entry.fence_value != g_dx_context->GetCurrentFenceValue());
      g_dx_context->WaitForFence(entry.fence_value);
      completed_fence_counter = g_dx_context->GetCompletedFenceValue();
    }

    // If this wrapped around, we need to flush the entries before the end of the buffer.
    if (index < m_query_readback_pos)
    {
      AccumulateQueriesFromBuffer(readback_count);
      DEBUG_ASSERT(m_query_readback_pos == 0);
      readback_count = 0;
    }

    readback_count++;
  }

  if (readback_count > 0)
    AccumulateQueriesFromBuffer(readback_count);
}

void PerfQuery::AccumulateQueriesFromBuffer(u32 query_count)
{
  // Should be at maximum query_count queries pending.
  ASSERT(query_count <= m_query_count.load(std::memory_order_relaxed) &&
         (m_query_readback_pos + query_count) <= PERF_QUERY_BUFFER_SIZE);

  const D3D12_RANGE read_range = {m_query_readback_pos * sizeof(PerfQueryDataType),
                                  (m_query_readback_pos + query_count) * sizeof(PerfQueryDataType)};
  u8* mapped_ptr;
  HRESULT hr = m_query_readback_buffer->Map(0, &read_range, reinterpret_cast<void**>(&mapped_ptr));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to map query readback buffer: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return;

  // Remove pending queries.
  for (u32 i = 0; i < query_count; i++)
  {
    u32 index = (m_query_readback_pos + i) % PERF_QUERY_BUFFER_SIZE;
    ActiveQuery& entry = m_query_buffer[index];

    // Should have a fence associated with it (waiting for a result).
    ASSERT(entry.fence_value != 0);
    entry.fence_value = 0;
    entry.resolved = false;
    entry.has_value = false;

    // Grab result from readback buffer, it will already have been invalidated.
    PerfQueryDataType result;
    std::memcpy(&result, mapped_ptr + (index * sizeof(PerfQueryDataType)), sizeof(result));

    // NOTE: Reported pixel metrics should be referenced to native resolution
    u64 native_res_result = static_cast<u64>(result) * EFB_WIDTH /
                            g_framebuffer_manager->GetEFBWidth() * EFB_HEIGHT /
                            g_framebuffer_manager->GetEFBHeight();
    if (g_ActiveConfig.iMultisamples > 1)
      native_res_result /= g_ActiveConfig.iMultisamples;
    m_results[entry.query_group].fetch_add(static_cast<u32>(native_res_result),
                                           std::memory_order_relaxed);
  }

  constexpr D3D12_RANGE write_range = {0, 0};
  m_query_readback_buffer->Unmap(0, &write_range);

  m_query_readback_pos = (m_query_readback_pos + query_count) % PERF_QUERY_BUFFER_SIZE;
  m_query_count.fetch_sub(query_count, std::memory_order_relaxed);
}

void PerfQuery::PartialFlush(bool resolve, bool blocking)
{
  // Submit a command buffer if there are unresolved queries (to write them to the buffer).
  if (resolve && m_unresolved_queries > 0)
    Gfx::GetInstance()->ExecuteCommandList(false);

  ReadbackQueries(blocking);
}
}  // namespace DX12
