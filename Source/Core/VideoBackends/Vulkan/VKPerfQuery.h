// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoCommon/PerfQueryBase.h"

namespace Vulkan
{
class PerfQuery : public PerfQueryBase
{
public:
  PerfQuery();
  ~PerfQuery();

  static PerfQuery* GetInstance() { return static_cast<PerfQuery*>(g_perf_query.get()); }

  bool Initialize();

  void EnableQuery(PerfQueryGroup type) override;
  void DisableQuery(PerfQueryGroup type) override;
  void ResetQuery() override;
  u32 GetQueryResult(PerfQueryType type) override;
  void FlushResults() override;
  bool IsFlushed() const override;

private:
  // u32 is used for the sample counts.
  using PerfQueryDataType = u32;

  // when testing in SMS: 64 was too small, 128 was ok
  // TODO: This should be size_t, but the base class uses u32s
  static const u32 PERF_QUERY_BUFFER_SIZE = 512;

  struct ActiveQuery
  {
    u64 fence_counter;
    PerfQueryType query_type;
    bool has_value;
  };

  bool CreateQueryPool();
  void ReadbackQueries();
  void ReadbackQueries(u32 query_count);
  void PartialFlush(bool blocking);

  VkQueryPool m_query_pool = VK_NULL_HANDLE;
  u32 m_query_readback_pos = 0;
  u32 m_query_next_pos = 0;
  std::array<ActiveQuery, PERF_QUERY_BUFFER_SIZE> m_query_buffer = {};
  std::array<PerfQueryDataType, PERF_QUERY_BUFFER_SIZE> m_query_result_buffer = {};
};

}  // namespace Vulkan
