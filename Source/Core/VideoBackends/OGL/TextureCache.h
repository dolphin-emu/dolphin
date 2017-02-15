// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"

#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{
class TextureCache : public TextureCacheBase
{
public:
  TextureCache();
  ~TextureCache();

  static void DisableStage(unsigned int stage);
  static void SetStage();

private:
  struct TCacheEntry : TCacheEntryBase
  {
    GLuint texture;
    GLuint framebuffer;

    // TexMode0 mode; // current filter and clamp modes that texture is set to
    // TexMode1 mode1; // current filter and clamp modes that texture is set to

    TCacheEntry(const TCacheEntryConfig& config);
    ~TCacheEntry();

    void CopyRectangleFromTexture(const TCacheEntryBase* source,
                                  const MathUtil::Rectangle<int>& srcrect,
                                  const MathUtil::Rectangle<int>& dstrect) override;

    void Load(const u8* buffer, u32 width, u32 height, u32 expanded_width, u32 level) override;

    void FromRenderTarget(bool is_depth_copy, const EFBRectangle& srcRect, bool scaleByHalf,
                          unsigned int cbufid, const float* colmat) override;

    void Bind(unsigned int stage) override;
    bool Save(const std::string& filename, unsigned int level) override;
  };

  TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;
  void ConvertTexture(TCacheEntryBase* entry, TCacheEntryBase* unconverted, void* palette,
                      TlutFormat format) override;

  void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
               u32 memory_stride, bool is_depth_copy, const EFBRectangle& srcRect, bool isIntensity,
               bool scaleByHalf) override;

  bool CompileShaders() override;
  void DeleteShaders() override;
};

bool SaveTexture(const std::string& filename, u32 textarget, u32 tex, int virtual_width,
                 int virtual_height, unsigned int level);
}
