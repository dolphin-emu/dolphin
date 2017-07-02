// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "VideoCommon/AbstractTexture.h"

class D3DTexture2D;

namespace DX11
{
class DXTexture final : public AbstractTexture
{
public:
  explicit DXTexture(const TextureConfig& tex_config);
  ~DXTexture();

  void Bind(unsigned int stage) override;
  bool Save(const std::string& filename, unsigned int level) override;

  void CopyRectangleFromTexture(const AbstractTexture* source,
                                const MathUtil::Rectangle<int>& srcrect,
                                const MathUtil::Rectangle<int>& dstrect) override;
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
            size_t buffer_size) override;

  D3DTexture2D* GetRawTexIdentifier() const;

private:
  D3DTexture2D* m_texture;
};

}  // namespace DX11
