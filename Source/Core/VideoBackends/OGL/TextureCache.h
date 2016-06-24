// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/AbstractTexture.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{
// TODO: Don't do this
extern SHADER s_ColorCopyProgram;
extern SHADER s_ColorMatrixProgram;
extern SHADER s_DepthMatrixProgram;
extern GLuint s_ColorMatrixUniform;
extern GLuint s_DepthMatrixUniform;
extern GLuint s_ColorCopyPositionUniform;
extern GLuint s_ColorMatrixPositionUniform;
extern GLuint s_DepthCopyPositionUniform;
extern u32 s_ColorCbufid;
extern u32 s_DepthCbufid;

class TextureCache : public TextureCacheBase
{
public:
  TextureCache();
  ~TextureCache();

private:
  std::unique_ptr<AbstractTextureBase>
  CreateTexture(const AbstractTextureBase::TextureConfig& config) override;
  void ConvertTexture(TCacheEntry* dest, TCacheEntry* soruce, void* palette,
                      TlutFormat format) override;

  void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
               u32 memory_stride, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
               bool isIntensity, bool scaleByHalf) override;

  void CompileShaders() override;
  void DeleteShaders() override;
};

bool SaveTexture(const std::string& filename, u32 textarget, u32 tex, int virtual_width,
                 int virtual_height, unsigned int level);
}
