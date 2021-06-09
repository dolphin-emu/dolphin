// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

#include "VideoCommon/BoundingBox.h"

namespace Null
{
class NullBoundingBox final : public BoundingBox
{
public:
  bool Initialize() override { return true; }

protected:
  std::vector<BBoxType> Read(u32 index, u32 length) override
  {
    return std::vector<BBoxType>(length);
  }
  void Write(u32 index, const std::vector<BBoxType>& values) override {}
};

}  // namespace Null
