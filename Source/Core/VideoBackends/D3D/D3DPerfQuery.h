// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoCommon/PerfQueryBase.h"

namespace DX11
{
class PerfQuery : public PerfQueryBase
{
public:
  PerfQuery();
  ~PerfQuery();

  void EnableQuery(PerfQueryGroup group) override;
  void DisableQuery(PerfQueryGroup group) override;
  void ResetQuery() override;
  u32 GetQueryResult(PerfQueryType type) override;
  void FlushResults() override;
  bool IsFlushed() const override;

private:
  struct ActiveQuery
  {
    ComPtr<ID3D11Query> query;
    PerfQueryGroup query_group{};
  };

  void WeakFlush();

  // Only use when non-empty
  void FlushOne();

  // when testing in SMS: 64 was too small, 128 was ok
  static constexpr int PERF_QUERY_BUFFER_SIZE = 512;

  std::array<ActiveQuery, PERF_QUERY_BUFFER_SIZE> m_query_buffer;
  int m_query_read_pos;
};

}  // namespace DX11
