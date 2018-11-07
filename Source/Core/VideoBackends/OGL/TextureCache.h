// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"

#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/TextureConverterShaderGen.h"
#include "VideoCommon/VideoCommon.h"

class AbstractTexture;
class StreamBuffer;
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
  struct PaletteShader
  {
    SHADER shader;
    GLuint buffer_offset_uniform;
    GLuint multiplier_uniform;
    GLuint copy_position_uniform;
  };

  struct TextureDecodingProgramInfo
  {
    const TextureConversionShaderTiled::DecodingShaderInfo* base_info = nullptr;
    SHADER program;
    GLint uniform_dst_size = -1;
    GLint uniform_src_size = -1;
    GLint uniform_src_row_stride = -1;
    GLint uniform_src_offset = -1;
    GLint uniform_palette_offset = -1;
    bool valid = false;
  };

  void ConvertTexture(TCacheEntry* destination, TCacheEntry* source, const void* palette,
                      TLUTFormat format) override;

  void CopyEFB(AbstractStagingTexture* dst, const EFBCopyParams& params, u32 native_width,
               u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
               bool scale_by_half, float y_scale, float gamma, bool clamp_top, bool clamp_bottom,
               const CopyFilterCoefficientArray& filter_coefficients) override;

  void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy, const EFBRectangle& src_rect,
                           bool scale_by_half, EFBCopyFormat dst_format, bool is_intensity,
                           float gamma, bool clamp_top, bool clamp_bottom,
                           const CopyFilterCoefficientArray& filter_coefficients) override;

  bool CompileShaders() override;
  void DeleteShaders() override;

  bool CompilePaletteShader(TLUTFormat tlutfmt, const std::string& vcode, const std::string& pcode,
                            const std::string& gcode);

  void CreateTextureDecodingResources();
  void DestroyTextureDecodingResources();

  struct EFBCopyShader
  {
    SHADER shader;
    GLuint position_uniform;
    GLuint pixel_height_uniform;
    GLuint gamma_rcp_uniform;
    GLuint clamp_tb_uniform;
    GLuint filter_coefficients_uniform;
  };

  std::map<TextureConversionShaderGen::TCShaderUid, EFBCopyShader> m_efb_copy_programs;

  SHADER m_colorCopyProgram;
  GLuint m_colorCopyPositionUniform;

  std::array<PaletteShader, 3> m_palette_shaders;
  std::unique_ptr<StreamBuffer> m_palette_stream_buffer;
  GLuint m_palette_resolv_texture = 0;

  std::map<std::pair<u32, u32>, TextureDecodingProgramInfo> m_texture_decoding_program_info;
  std::array<GLuint, TextureConversionShaderTiled::BUFFER_FORMAT_COUNT>
      m_texture_decoding_buffer_views;
};
}
