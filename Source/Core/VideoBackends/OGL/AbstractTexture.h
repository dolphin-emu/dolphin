// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/GL/GLUtil.h"

#include "VideoCommon/AbstractTextureBase.h"
#include "VideoCommon/BPStructs.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{
class AbstractTexture : public AbstractTextureBase
{
public:
  GLuint texture;
  GLuint framebuffer;

  AbstractTexture(const TextureConfig& config);
  ~AbstractTexture();

  void CopyRectangleFromTexture(const AbstractTextureBase* source,
                                const MathUtil::Rectangle<int>& srcrect,
                                const MathUtil::Rectangle<int>& dstrect) override;

  void Load(u8* data, unsigned int width, unsigned int height, unsigned int expanded_width,
            unsigned int level) override;

  void FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
                        bool scaleByHalf, unsigned int cbufid, const float* colmat) override;

  void Bind(unsigned int stage) override;
  bool Save(const std::string& filename, unsigned int level) override;

  // Some static functions to track bound textures.
  // TODO: Add DSO support for those drivers which support it.
  static void SetStage();
  static void InitializeBindingStateTracking()
  {
    s_ActiveTexture = -1;
    s_Textures.fill(-1);
  }

private:
  static GLint s_ActiveTexture;
  static std::array<GLint, 16> s_Textures;
};
}