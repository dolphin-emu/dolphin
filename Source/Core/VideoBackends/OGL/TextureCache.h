// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"

#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"

class AbstractTexture;
struct TextureConfig;

namespace OGL
{
class TextureCache : public TextureCacheBase
{
public:
  TextureCache();
  ~TextureCache();

  static TextureCache* GetInstance();

  bool SupportsGPUTextureDecode(TextureFormat format, TLUTFormat palette_format) override;
  void DecodeTextureOnGPU(TCacheEntry* entry, u32 dst_level, const u8* data, size_t data_size,
                          TextureFormat format, u32 width, u32 height, u32 aligned_width,
                          u32 aligned_height, u32 row_stride, const u8* palette,
                          TLUTFormat palette_format) override;

  const SHADER& GetColorCopyProgram() const;
  GLuint GetColorCopyPositionUniform() const;

private:
  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override;
  void ConvertTexture(TCacheEntry* destination, TCacheEntry* source, const void* palette,
                      TLUTFormat format) override;

  void CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width, u32 bytes_per_row,
               u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
               bool scale_by_half) override;

  void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy, const EFBRectangle& src_rect,
                           bool scale_by_half, unsigned int cbuf_id, const float* colmat) override;

  bool CompileShaders() override;
  void DeleteShaders() override;

  SHADER m_colorCopyProgram;
  SHADER m_colorMatrixProgram;
  SHADER m_depthMatrixProgram;
  GLuint m_colorMatrixUniform;
  GLuint m_depthMatrixUniform;
  GLuint m_colorCopyPositionUniform;
  GLuint m_colorMatrixPositionUniform;
  GLuint m_depthCopyPositionUniform;
};

bool SaveTexture(const std::string& filename, u32 textarget, u32 tex, int virtual_width,
                 int virtual_height, unsigned int level);
}
