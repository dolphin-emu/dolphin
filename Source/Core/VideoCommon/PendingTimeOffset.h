#pragma once

#include <mutex>

#include "Common/CommonTypes.h"

constexpr u64 TARGET_VSYNC_BLOCK_US = 2'000;

struct PendingTimeOffset
{
  std::mutex Lock;
  DT Offset__;
};

extern PendingTimeOffset s_pending_time_offset;
