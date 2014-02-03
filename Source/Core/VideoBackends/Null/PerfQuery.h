// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/PerfQueryBase.h"

namespace Null
{
class PerfQuery : public PerfQueryBase
{
public:
  PerfQuery() {}
  ~PerfQuery() override {}
  void EnableQuery(PerfQueryGroup type) override {}
  void DisableQuery(PerfQueryGroup type) override {}
  void ResetQuery() override {}
  u32 GetQueryResult(PerfQueryType type) override { return 0; }
  void FlushResults() override {}
  bool IsFlushed() const { return true; }
};

}  // namespace
