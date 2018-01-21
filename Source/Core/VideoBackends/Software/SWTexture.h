// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "VideoCommon/AbstractTexture.h"

namespace SW
{
class SWTexture final : public AbstractTexture
{
public:
  explicit SWTexture(const TextureConfig& tex_config);
  ~SWTexture() = default;

  void Bind(unsigned int stage) override;

  void CopyRectangleFromTexture(const AbstractTexture* source,
                                const MathUtil::Rectangle<int>& srcrect,
                                const MathUtil::Rectangle<int>& dstrect) override;
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
            size_t buffer_size) override;
};

}  // namespace SW
