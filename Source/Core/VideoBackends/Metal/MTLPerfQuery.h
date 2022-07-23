// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <condition_variable>
#include <mutex>

#include "VideoCommon/PerfQueryBase.h"

namespace Metal
{
class PerfQuery final : public PerfQueryBase
{
public:
  void EnableQuery(PerfQueryGroup type) override;
  void DisableQuery(PerfQueryGroup type) override;
  void ResetQuery() override;
  u32 GetQueryResult(PerfQueryType type) override;
  void FlushResults() override;
  bool IsFlushed() const override;

  /// Notify PerfQuery of a new pending encoder
  /// One call to ReturnResults should be made for every call to IncCount
  void IncCount() { m_query_count.fetch_add(1, std::memory_order_relaxed); }
  /// May be called from any thread
  void ReturnResults(const u64* data, const PerfQueryGroup* groups, size_t count, u32 query_id);

private:
  u32 m_current_query = 0;
  std::mutex m_results_mtx;
  std::condition_variable m_cv;
};
}  // namespace Metal
