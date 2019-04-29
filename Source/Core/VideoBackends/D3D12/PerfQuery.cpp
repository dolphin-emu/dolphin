// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/DXContext.h"
#include "VideoBackends/D3D12/PerfQuery.h"
#include "VideoBackends/D3D12/Renderer.h"

namespace DX12
{
PerfQuery::PerfQuery() = default;

PerfQuery::~PerfQuery() = default;

bool PerfQuery::Initialize()
{
  constexpr D3D12_QUERY_HEAP_DESC desc = {D3D12_QUERY_HEAP_TYPE_OCCLUSION, PERF_QUERY_BUFFER_SIZE};
  HRESULT hr = g_dx_context->GetDevice()->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_query_heap));
  CHECK(SUCCEEDED(hr), "Failed to create query heap");
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
  CHECK(SUCCEEDED(hr), "Failed to create query buffer");
  if (FAILED(hr))
    return false;

  return true;
}

void PerfQuery::EnableQuery(PerfQueryGroup type)
{
  // Block if there are no free slots.
  // Otherwise, try to keep half of them available.
  if (m_query_count > m_query_buffer.size() / 2)
  {
    const bool do_resolve = m_unresolved_queries > m_query_buffer.size() / 2;
    const bool blocking = m_query_count == PERF_QUERY_BUFFER_SIZE;
    PartialFlush(do_resolve, blocking);
  }

  // Ensure all state is applied before beginning the query.
  // This is because we can't leave a query open when submitting a command list, and the draw
  // call itself may need to execute a command list if we run out of descriptors. Note that
  // this assumes that the caller has bound all required state prior to enabling the query.
  Renderer::GetInstance()->ApplyState();

  if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
  {
    ActiveQuery& entry = m_query_buffer[m_query_next_pos];
    ASSERT(!entry.has_value && !entry.resolved);
    entry.has_value = true;

    g_dx_context->GetCommandList()->BeginQuery(m_query_heap.Get(), D3D12_QUERY_TYPE_OCCLUSION,
                                               m_query_next_pos);
  }
}

void PerfQuery::DisableQuery(PerfQueryGroup type)
{
  if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
  {
    g_dx_context->GetCommandList()->EndQuery(m_query_heap.Get(), D3D12_QUERY_TYPE_OCCLUSION,
                                             m_query_next_pos);
    m_query_next_pos = (m_query_next_pos + 1) % PERF_QUERY_BUFFER_SIZE;
    m_query_count++;
    m_unresolved_queries++;
  }
}

void PerfQuery::ResetQuery()
{
  m_query_count = 0;
  m_unresolved_queries = 0;
  m_query_resolve_pos = 0;
  m_query_readback_pos = 0;
  m_query_next_pos = 0;
  std::fill_n(m_results, ArraySize(m_results), 0);
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
    PartialFlush(true, true);
}

bool PerfQuery::IsFlushed() const
{
  return m_query_count == 0;
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
  const u32 outstanding_queries = m_query_count;
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
  ASSERT(query_count <= m_query_count &&
         (m_query_readback_pos + query_count) <= PERF_QUERY_BUFFER_SIZE);

  const D3D12_RANGE read_range = {m_query_readback_pos * sizeof(PerfQueryDataType),
                                  (m_query_readback_pos + query_count) * sizeof(PerfQueryDataType)};
  u8* mapped_ptr;
  HRESULT hr = m_query_readback_buffer->Map(0, &read_range, reinterpret_cast<void**>(&mapped_ptr));
  CHECK(SUCCEEDED(hr), "Failed to map query readback buffer");
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
    m_results[entry.query_type] +=
        static_cast<u32>(static_cast<u64>(result) * EFB_WIDTH / g_renderer->GetTargetWidth() *
                         EFB_HEIGHT / g_renderer->GetTargetHeight());
  }

  constexpr D3D12_RANGE write_range = {0, 0};
  m_query_readback_buffer->Unmap(0, &write_range);

  m_query_readback_pos = (m_query_readback_pos + query_count) % PERF_QUERY_BUFFER_SIZE;
  m_query_count -= query_count;
}

void PerfQuery::PartialFlush(bool resolve, bool blocking)
{
  // Submit a command buffer if there are unresolved queries (to write them to the buffer).
  if (resolve && m_unresolved_queries > 0)
    Renderer::GetInstance()->ExecuteCommandList(false);

  ReadbackQueries(blocking);
}
}  // namespace DX12
