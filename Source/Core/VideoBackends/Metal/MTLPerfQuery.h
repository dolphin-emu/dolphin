// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/PerfQueryBase.h"

namespace Metal
{
class PerfQuery final : public PerfQueryBase
{
public:
  void EnableQuery(PerfQueryGroup type) override {}
  void DisableQuery(PerfQueryGroup type) override {}
  void ResetQuery() override {}
  u32 GetQueryResult(PerfQueryType type) override { return 0; }
  void FlushResults() override {}
  bool IsFlushed() const override { return true; }
};
}  // namespace Metal
