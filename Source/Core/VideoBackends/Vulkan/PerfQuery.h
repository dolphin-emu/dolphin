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
class StagingBuffer;

class PerfQuery : public PerfQueryBase
{
public:
  PerfQuery();
  ~PerfQuery();

  static PerfQuery* GetInstance();

  bool Initialize();
  void FlushQueries();

  void EnableQuery(PerfQueryGroup type) override;
  void DisableQuery(PerfQueryGroup type) override;
  void ResetQuery() override;
  u32 GetQueryResult(PerfQueryType type) override;
  void FlushResults() override;
  bool IsFlushed() const override;

private:
  struct ActiveQuery
  {
    u64 fence_counter;
    PerfQueryType query_type;
    bool available;
    bool active;
  };

  bool CreateQueryPool();
  bool CreateReadbackBuffer();
  void QueueCopyQueryResults(u32 start_index, u32 query_count);
  void ProcessPendingResults();
  void ProcessResults(u32 start_index, u32 query_count);

  void NonBlockingPartialFlush();
  void BlockingPartialFlush();

  // when testing in SMS: 64 was too small, 128 was ok
  // TODO: This should be size_t, but the base class uses u32s
  using PerfQueryDataType = u32;
  static const u32 PERF_QUERY_BUFFER_SIZE = 512;
  std::array<ActiveQuery, PERF_QUERY_BUFFER_SIZE> m_query_buffer = {};
  u32 m_query_read_pos = 0;

  // TODO: Investigate using pipeline statistics to implement other query types
  VkQueryPool m_query_pool = VK_NULL_HANDLE;

  // Buffer containing query results. Each query is a u32.
  std::unique_ptr<StagingBuffer> m_readback_buffer;
};

}  // namespace Vulkan
