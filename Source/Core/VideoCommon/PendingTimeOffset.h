#pragma once

#include <mutex>

#include "Common/CommonTypes.h"

// Setting this value too low will cause repeated dropped frames
// as emulation constantly slows down too late for vsync.
constexpr u64 TARGET_VSYNC_BLOCK_US = 4'000;

struct PendingTimeOffset
{
  std::mutex Lock;
  DT Offset__;
};

extern PendingTimeOffset s_pending_time_offset;
