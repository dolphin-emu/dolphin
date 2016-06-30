// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoCommon/AbstractTextureBase.h"

namespace DX11
{

class AbstractTexture : public AbstractTextureBase
{
public:
  D3DTexture2D* texture;

  D3D11_USAGE usage;

  AbstractTexture(const AbstractTextureBase::TextureConfig& config);
  ~AbstractTexture();

  void CopyRectangleFromTexture(const AbstractTextureBase* source,
                                const MathUtil::Rectangle<int>& srcrect,
                                const MathUtil::Rectangle<int>& dstrect) override;

  void Load(u8* data, unsigned int width, unsigned int height, unsigned int expanded_width,
            unsigned int levels) override;

  void FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
                        bool scaleByHalf, unsigned int cbufid, const float* colmat) override;

  void Bind(unsigned int stage) override;
  bool Save(const std::string& filename, unsigned int level) override;

  static void Shutdown();
};

}
