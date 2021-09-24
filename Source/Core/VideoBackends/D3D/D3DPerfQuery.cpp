// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D/D3DPerfQuery.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoCommon.h"

namespace DX11
{
PerfQuery::PerfQuery() : m_query_read_pos()
{
  for (ActiveQuery& entry : m_query_buffer)
  {
    D3D11_QUERY_DESC qdesc = CD3D11_QUERY_DESC(D3D11_QUERY_OCCLUSION, 0);
    D3D::device->CreateQuery(&qdesc, &entry.query);
  }

  ResetQuery();
}

PerfQuery::~PerfQuery() = default;

void PerfQuery::EnableQuery(PerfQueryGroup type)
{
  u32 query_count = m_query_count.load(std::memory_order_relaxed);

  // Is this sane?
  if (query_count > m_query_buffer.size() / 2)
  {
    WeakFlush();
    query_count = m_query_count.load(std::memory_order_relaxed);
  }

  if (m_query_buffer.size() == query_count)
  {
    // TODO
    FlushOne();
    query_count = m_query_count.load(std::memory_order_relaxed);
    ERROR_LOG_FMT(VIDEO, "Flushed query buffer early!");
  }

  // start query
  if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
  {
    auto& entry = m_query_buffer[(m_query_read_pos + query_count) % m_query_buffer.size()];

    D3D::context->Begin(entry.query.Get());
    entry.query_type = type;

    m_query_count.fetch_add(1, std::memory_order_relaxed);
  }
}

void PerfQuery::DisableQuery(PerfQueryGroup type)
{
  // stop query
  if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
  {
    auto& entry = m_query_buffer[(m_query_read_pos + m_query_count.load(std::memory_order_relaxed) +
                                  m_query_buffer.size() - 1) %
                                 m_query_buffer.size()];
    D3D::context->End(entry.query.Get());
  }
}

void PerfQuery::ResetQuery()
{
  m_query_count.store(0, std::memory_order_relaxed);
  for (size_t i = 0; i < m_results.size(); ++i)
    m_results[i].store(0, std::memory_order_relaxed);
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

  return result;
}

void PerfQuery::FlushOne()
{
  auto& entry = m_query_buffer[m_query_read_pos];

  UINT64 result = 0;
  HRESULT hr = S_FALSE;
  while (hr != S_OK)
  {
    // TODO: Might cause us to be stuck in an infinite loop!
    hr = D3D::context->GetData(entry.query.Get(), &result, sizeof(result), 0);
  }

  // NOTE: Reported pixel metrics should be referenced to native resolution
  // TODO: Dropping the lower 2 bits from this count should be closer to actual
  // hardware behavior when drawing triangles.
  const u64 native_res_result = result * EFB_WIDTH / g_renderer->GetTargetWidth() * EFB_HEIGHT /
                                g_renderer->GetTargetHeight();
  m_results[entry.query_type].fetch_add(static_cast<u32>(native_res_result),
                                        std::memory_order_relaxed);

  m_query_read_pos = (m_query_read_pos + 1) % m_query_buffer.size();
  m_query_count.fetch_sub(1, std::memory_order_relaxed);
}

// TODO: could selectively flush things, but I don't think that will do much
void PerfQuery::FlushResults()
{
  while (!IsFlushed())
    FlushOne();
}

void PerfQuery::WeakFlush()
{
  while (!IsFlushed())
  {
    auto& entry = m_query_buffer[m_query_read_pos];

    UINT64 result = 0;
    HRESULT hr = D3D::context->GetData(entry.query.Get(), &result, sizeof(result),
                                       D3D11_ASYNC_GETDATA_DONOTFLUSH);

    if (hr == S_OK)
    {
      // NOTE: Reported pixel metrics should be referenced to native resolution
      const u64 native_res_result = result * EFB_WIDTH / g_renderer->GetTargetWidth() * EFB_HEIGHT /
                                    g_renderer->GetTargetHeight();
      m_results[entry.query_type].store(static_cast<u32>(native_res_result),
                                        std::memory_order_relaxed);

      m_query_read_pos = (m_query_read_pos + 1) % m_query_buffer.size();
      m_query_count.fetch_sub(1, std::memory_order_relaxed);
    }
    else
    {
      break;
    }
  }
}

bool PerfQuery::IsFlushed() const
{
  return m_query_count.load(std::memory_order_relaxed) == 0;
}

}  // namespace DX11
