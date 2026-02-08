// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include "VideoBackends/D3D12/DX12Context.h"
#include "VideoCommon/PerfQueryBase.h"

namespace DX12
{
class PerfQuery final : public PerfQueryBase
{
public:
  PerfQuery();
  ~PerfQuery();

  static PerfQuery* GetInstance() { return static_cast<PerfQuery*>(g_perf_query.get()); }

  bool Initialize() override;
  void ResolveQueries();

  void EnableQuery(PerfQueryGroup group) override;
  void DisableQuery(PerfQueryGroup group) override;
  void ResetQuery() override;
  u32 GetQueryResult(PerfQueryType type) override;
  void FlushResults() override;
  bool IsFlushed() const override;

private:
  struct ActiveQuery
  {
    u64 fence_value;
    PerfQueryGroup query_group;
    bool has_value;
    bool resolved;
  };

  void ResolveQueries(u32 query_count);
  void ReadbackQueries(bool blocking);
  void AccumulateQueriesFromBuffer(u32 query_count);

  void PartialFlush(bool resolve, bool blocking);

  // when testing in SMS: 64 was too small, 128 was ok
  // TODO: This should be size_t, but the base class uses u32s
  using PerfQueryDataType = u64;
  static const u32 PERF_QUERY_BUFFER_SIZE = 512;
  std::array<ActiveQuery, PERF_QUERY_BUFFER_SIZE> m_query_buffer = {};
  u32 m_unresolved_queries = 0;
  u32 m_query_resolve_pos = 0;
  u32 m_query_readback_pos = 0;
  u32 m_query_next_pos = 0;

  ComPtr<ID3D12QueryHeap> m_query_heap;
  ComPtr<ID3D12Resource> m_query_readback_buffer;
};

}  // namespace DX12
