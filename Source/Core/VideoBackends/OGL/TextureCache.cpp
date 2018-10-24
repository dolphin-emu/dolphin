// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/GPUTimer.h"
#include "VideoBackends/OGL/OGLTexture.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/StreamBuffer.h"
#include "VideoBackends/OGL/TextureCache.h"
#include "VideoBackends/OGL/TextureConverter.h"

#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/TextureConverterShaderGen.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
constexpr const char GLSL_PROGRAM_VS[] = R"GLSL(
out vec3 %c_uv0;
SAMPLER_BINDING(9) uniform sampler2DArray samp9;
uniform vec4 copy_position;  // left, top, right, bottom

void main()
{
  vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);
  %c_uv0 = vec3(mix(copy_position.xy, copy_position.zw, rawpos) / vec2(textureSize(samp9, 0).xy), 0.0);
  gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);
}
)GLSL";

constexpr const char GLSL_PROGRAM_GS[] = R"GLSL(
layout(triangles) in;
layout(triangle_strip, max_vertices = 6) out;
in vec3 v_uv0[3];
out vec3 f_uv0;
SAMPLER_BINDING(9) uniform sampler2DArray samp9;

void main()
{
  int layers = textureSize(samp9, 0).z;
  for (int layer = 0; layer < layers; ++layer) {
    for (int i = 0; i < 3; ++i) {
      f_uv0 = vec3(v_uv0[i].xy, layer);
      gl_Position = gl_in[i].gl_Position;
      gl_Layer = layer;
      EmitVertex();
  }
  EndPrimitive();
}
)GLSL";

constexpr const char GLSL_COLOR_COPY_FS[] = R"GLSL(
SAMPLER_BINDING(9) uniform sampler2DArray samp9;
in vec3 f_uv0;
out vec4 ocol0;

void main()
{
  vec4 texcol = texture(samp9, f_uv0);
  ocol0 = texcol;
}
)GLSL";

constexpr const char GLSL_PALETTE_FS[] = R"GLSL(
uniform int texture_buffer_offset;
uniform float multiplier;
SAMPLER_BINDING(9) uniform sampler2DArray samp9;
SAMPLER_BINDING(10) uniform usamplerBuffer samp10;

in vec3 f_uv0;
out vec4 ocol0;

int Convert3To8(int v)
{
  // Swizzle bits: 00000123 -> 12312312
  return (v << 5) | (v << 2) | (v >> 1);
}

int Convert4To8(int v)
{
  // Swizzle bits: 00001234 -> 12341234
  return (v << 4) | v;
}

int Convert5To8(int v)
{
  // Swizzle bits: 00012345 -> 12345123
  return (v << 3) | (v >> 2);
}

int Convert6To8(int v)
{
  // Swizzle bits: 00123456 -> 12345612
  return (v << 2) | (v >> 4);
}

float4 DecodePixel_RGB5A3(int val)
{
  int r,g,b,a;
  if ((val&0x8000) > 0)
  {
    r=Convert5To8((val>>10) & 0x1f);
    g=Convert5To8((val>>5 ) & 0x1f);
    b=Convert5To8((val    ) & 0x1f);
    a=0xFF;
  }
  else
  {
    a=Convert3To8((val>>12) & 0x7);
    r=Convert4To8((val>>8 ) & 0xf);
    g=Convert4To8((val>>4 ) & 0xf);
    b=Convert4To8((val    ) & 0xf);
  }
  return float4(r, g, b, a) / 255.0;
}

float4 DecodePixel_RGB565(int val)
{
  int r, g, b, a;
  r = Convert5To8((val >> 11) & 0x1f);
  g = Convert6To8((val >> 5) & 0x3f);
  b = Convert5To8((val) & 0x1f);
  a = 0xFF;
  return float4(r, g, b, a) / 255.0;
}

float4 DecodePixel_IA8(int val)
{
  int i = val & 0xFF;
  int a = val >> 8;
  return float4(i, i, i, a) / 255.0;
}

void main()
{
  int src = int(round(texture(samp9, f_uv0).r * multiplier));
  src = int(texelFetch(samp10, src + texture_buffer_offset).r);
  src = ((src << 8) & 0xFF00) | (src >> 8);
  ocol0 = DecodePixel_%s(src);
}
)GLSL";

//#define TIME_TEXTURE_DECODING 1

void TextureCache::CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width,
                           u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
                           const EFBRectangle& src_rect, bool scale_by_half, float y_scale,
                           float gamma, bool clamp_top, bool clamp_bottom,
                           const CopyFilterCoefficientArray& filter_coefficients)
{
  // Flip top/bottom due to lower-left coordinate system.
  float clamp_top_val =
      clamp_bottom ? (1.0f - src_rect.bottom / static_cast<float>(EFB_HEIGHT)) : 0.0f;
  float clamp_bottom_val =
      clamp_top ? (1.0f - src_rect.top / static_cast<float>(EFB_HEIGHT)) : 1.0f;
  TextureConverter::EncodeToRamFromTexture(dst, params, native_width, bytes_per_row, num_blocks_y,
                                           memory_stride, src_rect, scale_by_half, y_scale, gamma,
                                           clamp_top_val, clamp_bottom_val, filter_coefficients);
}

TextureCache::TextureCache()
{
  CompileShaders();

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
  {
    s32 buffer_size_mb = (g_ActiveConfig.backend_info.bSupportsGPUTextureDecoding ? 32 : 1);
    s32 buffer_size = buffer_size_mb * 1024 * 1024;
    s32 max_buffer_size = 0;

    // The minimum MAX_TEXTURE_BUFFER_SIZE that the spec mandates is 65KB, we are asking for a 1MB
    // buffer here. This buffer is also used as storage for undecoded textures when compute shader
    // texture decoding is enabled, in which case the requested size is 32MB.
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &max_buffer_size);

    // Clamp the buffer size to the maximum size that the driver supports.
    buffer_size = std::min(buffer_size, max_buffer_size);

    m_palette_stream_buffer = StreamBuffer::Create(GL_TEXTURE_BUFFER, buffer_size);
    glGenTextures(1, &m_palette_resolv_texture);
    glBindTexture(GL_TEXTURE_BUFFER, m_palette_resolv_texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R16UI, m_palette_stream_buffer->m_buffer);

    if (g_ActiveConfig.backend_info.bSupportsGPUTextureDecoding)
      CreateTextureDecodingResources();
  }
}

TextureCache::~TextureCache()
{
  DeleteShaders();
  if (g_ActiveConfig.backend_info.bSupportsGPUTextureDecoding)
    DestroyTextureDecodingResources();

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
  {
    glDeleteTextures(1, &m_palette_resolv_texture);
  }
}

TextureCache* TextureCache::GetInstance()
{
  return static_cast<TextureCache*>(g_texture_cache.get());
}

const SHADER& TextureCache::GetColorCopyProgram() const
{
  return m_colorCopyProgram;
}

GLuint TextureCache::GetColorCopyPositionUniform() const
{
  return m_colorCopyPositionUniform;
}

bool TextureCache::CompilePaletteShader(TLUTFormat tlutfmt, const std::string& vcode,
                                        const std::string& pcode, const std::string& gcode)
{
  ASSERT(IsValidTLUTFormat(tlutfmt));
  PaletteShader& shader = m_palette_shaders[static_cast<int>(tlutfmt)];

  if (!ProgramShaderCache::CompileShader(shader.shader, vcode, pcode, gcode))
    return false;

  shader.buffer_offset_uniform =
      glGetUniformLocation(shader.shader.glprogid, "texture_buffer_offset");
  shader.multiplier_uniform = glGetUniformLocation(shader.shader.glprogid, "multiplier");
  shader.copy_position_uniform = glGetUniformLocation(shader.shader.glprogid, "copy_position");

  return true;
}

bool TextureCache::CompileShaders()
{
  std::string geo_program = "";
  char prefix = 'f';
  if (g_ActiveConfig.stereo_mode != StereoMode::Off)
  {
    geo_program = GLSL_PROGRAM_GS;
    prefix = 'v';
  }

  if (!ProgramShaderCache::CompileShader(m_colorCopyProgram,
                                         StringFromFormat(GLSL_PROGRAM_VS, prefix, prefix),
                                         GLSL_COLOR_COPY_FS, geo_program))
  {
    return false;
  }

  m_colorCopyPositionUniform = glGetUniformLocation(m_colorCopyProgram.glprogid, "copy_position");

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
  {
    if (!CompilePaletteShader(TLUTFormat::IA8, StringFromFormat(GLSL_PROGRAM_VS, prefix, prefix),
                              StringFromFormat(GLSL_PALETTE_FS, "IA8"), geo_program))
      return false;

    if (!CompilePaletteShader(TLUTFormat::RGB565, StringFromFormat(GLSL_PROGRAM_VS, prefix, prefix),
                              StringFromFormat(GLSL_PALETTE_FS, "RGB565"), geo_program))
      return false;

    if (!CompilePaletteShader(TLUTFormat::RGB5A3, StringFromFormat(GLSL_PROGRAM_VS, prefix, prefix),
                              StringFromFormat(GLSL_PALETTE_FS, "RGB5A3"), geo_program))
      return false;
  }

  return true;
}

void TextureCache::DeleteShaders()
{
  for (auto& it : m_efb_copy_programs)
    it.second.shader.Destroy();
  m_efb_copy_programs.clear();

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
    for (auto& shader : m_palette_shaders)
      shader.shader.Destroy();
}

void TextureCache::ConvertTexture(TCacheEntry* destination, TCacheEntry* source,
                                  const void* palette, TLUTFormat tlutfmt)
{
  if (!g_ActiveConfig.backend_info.bSupportsPaletteConversion)
    return;

  ASSERT(IsValidTLUTFormat(tlutfmt));
  const PaletteShader& palette_shader = m_palette_shaders[static_cast<int>(tlutfmt)];

  g_renderer->ResetAPIState();

  OGLTexture* source_texture = static_cast<OGLTexture*>(source->texture.get());
  OGLTexture* destination_texture = static_cast<OGLTexture*>(destination->texture.get());

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, source_texture->GetRawTexIdentifier());
  g_sampler_cache->BindNearestSampler(9);

  FramebufferManager::SetFramebuffer(destination_texture->GetFramebuffer());
  glViewport(0, 0, destination->GetWidth(), destination->GetHeight());
  palette_shader.shader.Bind();

  // C14 textures are currently unsupported
  int size = source->format == TextureFormat::I4 ? 32 : 512;
  auto buffer = m_palette_stream_buffer->Map(size);
  memcpy(buffer.first, palette, size);
  m_palette_stream_buffer->Unmap(size);
  glUniform1i(palette_shader.buffer_offset_uniform, buffer.second / 2);
  glUniform1f(palette_shader.multiplier_uniform,
              source->format == TextureFormat::I4 ? 15.0f : 255.0f);
  glUniform4f(palette_shader.copy_position_uniform, 0.0f, 0.0f,
              static_cast<float>(source->GetWidth()), static_cast<float>(source->GetHeight()));

  glActiveTexture(GL_TEXTURE10);
  glBindTexture(GL_TEXTURE_BUFFER, m_palette_resolv_texture);
  g_sampler_cache->BindNearestSampler(10);

  ProgramShaderCache::BindVertexFormat(nullptr);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  g_renderer->RestoreAPIState();
}

static const std::string decoding_vertex_shader = R"(
void main()
{
  vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);
  gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);
}
)";

void TextureCache::CreateTextureDecodingResources()
{
  static const GLenum gl_view_types[TextureConversionShaderTiled::BUFFER_FORMAT_COUNT] = {
      GL_R8UI,     // BUFFER_FORMAT_R8_UINT
      GL_R16UI,    // BUFFER_FORMAT_R16_UINT
      GL_RG32UI,   // BUFFER_FORMAT_R32G32_UINT
      GL_RGBA8UI,  // BUFFER_FORMAT_RGBA8_UINT
  };

  glGenTextures(TextureConversionShaderTiled::BUFFER_FORMAT_COUNT,
                m_texture_decoding_buffer_views.data());
  for (size_t i = 0; i < TextureConversionShaderTiled::BUFFER_FORMAT_COUNT; i++)
  {
    glBindTexture(GL_TEXTURE_BUFFER, m_texture_decoding_buffer_views[i]);
    glTexBuffer(GL_TEXTURE_BUFFER, gl_view_types[i], m_palette_stream_buffer->m_buffer);
  }
}

void TextureCache::DestroyTextureDecodingResources()
{
  glDeleteTextures(TextureConversionShaderTiled::BUFFER_FORMAT_COUNT,
                   m_texture_decoding_buffer_views.data());
  m_texture_decoding_buffer_views.fill(0);
  m_texture_decoding_program_info.clear();
}

bool TextureCache::SupportsGPUTextureDecode(TextureFormat format, TLUTFormat palette_format)
{
  auto key = std::make_pair(static_cast<u32>(format), static_cast<u32>(palette_format));
  auto iter = m_texture_decoding_program_info.find(key);
  if (iter != m_texture_decoding_program_info.end())
    return iter->second.valid;

  TextureDecodingProgramInfo info;
  info.base_info = TextureConversionShaderTiled::GetDecodingShaderInfo(format);
  if (!info.base_info)
  {
    m_texture_decoding_program_info.emplace(key, info);
    return false;
  }

  std::string shader_source =
      TextureConversionShaderTiled::GenerateDecodingShader(format, palette_format, APIType::OpenGL);
  if (shader_source.empty())
  {
    m_texture_decoding_program_info.emplace(key, info);
    return false;
  }

  if (!ProgramShaderCache::CompileComputeShader(info.program, shader_source))
  {
    m_texture_decoding_program_info.emplace(key, info);
    return false;
  }

  info.uniform_dst_size = glGetUniformLocation(info.program.glprogid, "u_dst_size");
  info.uniform_src_size = glGetUniformLocation(info.program.glprogid, "u_src_size");
  info.uniform_src_offset = glGetUniformLocation(info.program.glprogid, "u_src_offset");
  info.uniform_src_row_stride = glGetUniformLocation(info.program.glprogid, "u_src_row_stride");
  info.uniform_palette_offset = glGetUniformLocation(info.program.glprogid, "u_palette_offset");
  info.valid = true;
  m_texture_decoding_program_info.emplace(key, info);
  return true;
}

void TextureCache::DecodeTextureOnGPU(TCacheEntry* entry, u32 dst_level, const u8* data,
                                      size_t data_size, TextureFormat format, u32 width, u32 height,
                                      u32 aligned_width, u32 aligned_height, u32 row_stride,
                                      const u8* palette, TLUTFormat palette_format)
{
  auto key = std::make_pair(static_cast<u32>(format), static_cast<u32>(palette_format));
  auto iter = m_texture_decoding_program_info.find(key);
  if (iter == m_texture_decoding_program_info.end())
    return;

#ifdef TIME_TEXTURE_DECODING
  GPUTimer timer;
#endif

  // Copy to GPU-visible buffer, aligned to the data type.
  auto info = iter->second;
  u32 bytes_per_buffer_elem =
      TextureConversionShaderTiled::GetBytesPerBufferElement(info.base_info->buffer_format);

  // Only copy palette if it is required.
  bool has_palette = info.base_info->palette_size > 0;
  u32 total_upload_size = static_cast<u32>(data_size);
  u32 palette_offset = total_upload_size;
  if (has_palette)
  {
    // Align to u16.
    if ((total_upload_size % sizeof(u16)) != 0)
    {
      total_upload_size++;
      palette_offset++;
    }

    total_upload_size += info.base_info->palette_size;
  }

  // Allocate space in stream buffer, and copy texture + palette across.
  auto buffer = m_palette_stream_buffer->Map(total_upload_size, bytes_per_buffer_elem);
  memcpy(buffer.first, data, data_size);
  if (has_palette)
    memcpy(buffer.first + palette_offset, palette, info.base_info->palette_size);
  m_palette_stream_buffer->Unmap(total_upload_size);

  info.program.Bind();

  // Calculate stride in buffer elements
  u32 row_stride_in_elements = row_stride / bytes_per_buffer_elem;
  u32 offset_in_elements = buffer.second / bytes_per_buffer_elem;
  u32 palette_offset_in_elements = (buffer.second + palette_offset) / sizeof(u16);
  if (info.uniform_dst_size >= 0)
    glUniform2ui(info.uniform_dst_size, width, height);
  if (info.uniform_src_size >= 0)
    glUniform2ui(info.uniform_src_size, aligned_width, aligned_height);
  if (info.uniform_src_offset >= 0)
    glUniform1ui(info.uniform_src_offset, offset_in_elements);
  if (info.uniform_src_row_stride >= 0)
    glUniform1ui(info.uniform_src_row_stride, row_stride_in_elements);
  if (info.uniform_palette_offset >= 0)
    glUniform1ui(info.uniform_palette_offset, palette_offset_in_elements);

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_BUFFER, m_texture_decoding_buffer_views[info.base_info->buffer_format]);

  if (has_palette)
  {
    // Use an R16UI view for the palette.
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_BUFFER, m_palette_resolv_texture);
  }

  auto dispatch_groups =
      TextureConversionShaderTiled::GetDispatchCount(info.base_info, aligned_width, aligned_height);
  glBindImageTexture(0, static_cast<OGLTexture*>(entry->texture.get())->GetRawTexIdentifier(),
                     dst_level, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
  glDispatchCompute(dispatch_groups.first, dispatch_groups.second, 1);
  glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

#ifdef TIME_TEXTURE_DECODING
  WARN_LOG(VIDEO, "Decode texture format %u size %ux%u took %.4fms", static_cast<u32>(format),
           width, height, timer.GetTimeMilliseconds());
#endif
}

void TextureCache::CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy,
                                       const EFBRectangle& src_rect, bool scale_by_half,
                                       EFBCopyFormat dst_format, bool is_intensity, float gamma,
                                       bool clamp_top, bool clamp_bottom,
                                       const CopyFilterCoefficientArray& filter_coefficients)
{
  auto* destination_texture = static_cast<OGLTexture*>(entry->texture.get());
  g_renderer->ResetAPIState();  // reset any game specific settings

  // Make sure to resolve anything we need to read from.
  const GLuint read_texture = is_depth_copy ?
                                  FramebufferManager::ResolveAndGetDepthTarget(src_rect) :
                                  FramebufferManager::ResolveAndGetRenderTarget(src_rect);

  FramebufferManager::SetFramebuffer(destination_texture->GetFramebuffer());

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, read_texture);
  if (scale_by_half)
    g_sampler_cache->BindLinearSampler(9);
  else
    g_sampler_cache->BindNearestSampler(9);

  glViewport(0, 0, destination_texture->GetConfig().width, destination_texture->GetConfig().height);

  auto uid = TextureConversionShaderGen::GetShaderUid(dst_format, is_depth_copy, is_intensity,
                                                      scale_by_half,
                                                      NeedsCopyFilterInShader(filter_coefficients));

  auto it = m_efb_copy_programs.emplace(uid, EFBCopyShader());
  EFBCopyShader& shader = it.first->second;
  bool created = it.second;

  if (created)
  {
    ShaderCode code = TextureConversionShaderGen::GenerateShader(APIType::OpenGL, uid.GetUidData());

    std::string geo_program = "";
    char prefix = 'f';
    if (g_ActiveConfig.stereo_mode != StereoMode::Off)
    {
      geo_program = GLSL_PROGRAM_GS;
      prefix = 'v';
    }

    ProgramShaderCache::CompileShader(shader.shader,
                                      StringFromFormat(GLSL_PROGRAM_VS, prefix, prefix),
                                      code.GetBuffer(), geo_program);

    shader.position_uniform = glGetUniformLocation(shader.shader.glprogid, "copy_position");
    shader.pixel_height_uniform = glGetUniformLocation(shader.shader.glprogid, "pixel_height");
    shader.gamma_rcp_uniform = glGetUniformLocation(shader.shader.glprogid, "gamma_rcp");
    shader.clamp_tb_uniform = glGetUniformLocation(shader.shader.glprogid, "clamp_tb");
    shader.filter_coefficients_uniform =
        glGetUniformLocation(shader.shader.glprogid, "filter_coefficients");
  }

  shader.shader.Bind();

  TargetRectangle R = g_renderer->ConvertEFBRectangle(src_rect);
  glUniform4f(shader.position_uniform, static_cast<float>(R.left), static_cast<float>(R.top),
              static_cast<float>(R.right), static_cast<float>(R.bottom));
  glUniform1f(shader.pixel_height_uniform, g_ActiveConfig.bCopyEFBScaled ?
                                               1.0f / g_renderer->GetTargetHeight() :
                                               1.0f / EFB_HEIGHT);
  glUniform1f(shader.gamma_rcp_uniform, 1.0f / gamma);
  glUniform2f(shader.clamp_tb_uniform,
              clamp_bottom ? (1.0f - src_rect.bottom / static_cast<float>(EFB_HEIGHT)) : 0.0f,
              clamp_top ? (1.0f - src_rect.top / static_cast<float>(EFB_HEIGHT)) : 1.0f);
  glUniform3f(shader.filter_coefficients_uniform, filter_coefficients[0], filter_coefficients[1],
              filter_coefficients[2]);

  ProgramShaderCache::BindVertexFormat(nullptr);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  g_renderer->RestoreAPIState();
}
}  // namespace OGL
