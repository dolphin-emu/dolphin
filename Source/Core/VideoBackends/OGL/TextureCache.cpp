// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <vector>

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"

#include "Common/GL/GLInterfaceBase.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/StreamBuffer.h"
#include "VideoBackends/OGL/TextureCache.h"
#include "VideoBackends/OGL/TextureConverter.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
static SHADER s_ColorCopyProgram;
static SHADER s_ColorMatrixProgram;
static SHADER s_DepthMatrixProgram;
static GLuint s_ColorMatrixUniform;
static GLuint s_DepthMatrixUniform;
static GLuint s_ColorCopyPositionUniform;
static GLuint s_ColorMatrixPositionUniform;
static GLuint s_DepthCopyPositionUniform;
static u32 s_ColorCbufid;
static u32 s_DepthCbufid;

static u32 s_Textures[8];
static u32 s_ActiveTexture;

static SHADER s_palette_pixel_shader[3];
static std::unique_ptr<StreamBuffer> s_palette_stream_buffer;
static GLuint s_palette_resolv_texture;
static GLuint s_palette_buffer_offset_uniform[3];
static GLuint s_palette_multiplier_uniform[3];
static GLuint s_palette_copy_position_uniform[3];

struct TextureDecodingProgramInfo
{
  const TextureConversionShader::TextureDecodingShaderInfo* base_info = nullptr;
  SHADER program;
  GLint uniform_dst_size = -1;
  GLint uniform_src_size = -1;
  GLint uniform_src_row_stride = -1;
  GLint uniform_src_offset = -1;
  bool valid = false;
};

static std::map<u32, TextureDecodingProgramInfo> s_texture_decoding_program_info;
static std::array<GLuint, TextureConversionShader::BUFFER_FORMAT_COUNT>
    s_texture_decoding_buffer_views;
static GLuint s_texture_decoding_framebuffer = 0;
static bool s_texture_decoding_use_compute_shader = true;
static void CreateTextureDecodingResources();
static void DestroyTextureDecodingResources();

bool SaveTexture(const std::string& filename, u32 textarget, u32 tex, int virtual_width,
                 int virtual_height, unsigned int level)
{
  if (GLInterface->GetMode() != GLInterfaceMode::MODE_OPENGL)
    return false;
  int width = std::max(virtual_width >> level, 1);
  int height = std::max(virtual_height >> level, 1);
  std::vector<u8> data(width * height * 4);
  glActiveTexture(GL_TEXTURE9);
  glBindTexture(textarget, tex);
  glGetTexImage(textarget, level, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
  TextureCache::SetStage();

  return TextureToPng(data.data(), width * 4, filename, width, height, true);
}

TextureCache::TCacheEntry::~TCacheEntry()
{
  if (texture)
  {
    for (auto& gtex : s_Textures)
      if (gtex == texture)
        gtex = 0;
    glDeleteTextures(1, &texture);
    texture = 0;
  }

  if (framebuffer)
  {
    glDeleteFramebuffers(1, &framebuffer);
    framebuffer = 0;
  }
}

TextureCache::TCacheEntry::TCacheEntry(const TCacheEntryConfig& _config) : TCacheEntryBase(_config)
{
  glGenTextures(1, &texture);

  framebuffer = 0;
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
  if (s_Textures[stage] != texture)
  {
    if (s_ActiveTexture != stage)
    {
      glActiveTexture(GL_TEXTURE0 + stage);
      s_ActiveTexture = stage;
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    s_Textures[stage] = texture;
  }
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, unsigned int level)
{
  return SaveTexture(filename, GL_TEXTURE_2D_ARRAY, texture, config.width, config.height, level);
}

TextureCache::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
  TCacheEntry* entry = new TCacheEntry(config);

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, entry->texture);

  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, config.levels - 1);

  if (g_ogl_config.bSupportsTextureStorage)
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, config.levels, GL_RGBA8, config.width, config.height, 1);

  if (config.rendertarget)
  {
    if (!g_ogl_config.bSupportsTextureStorage)
    {
      for (u32 level = 0; level < config.levels; level++)
      {
        glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA, std::max(config.width >> level, 1u),
                     std::max(config.height >> level, 1u), config.layers, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
      }
    }
    glGenFramebuffers(1, &entry->framebuffer);
    FramebufferManager::SetFramebuffer(entry->framebuffer);
    FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D_ARRAY, entry->texture, 0);
  }

  TextureCache::SetStage();
  return entry;
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(const TCacheEntryBase* source,
                                                         const MathUtil::Rectangle<int>& srcrect,
                                                         const MathUtil::Rectangle<int>& dstrect)
{
  TCacheEntry* srcentry = (TCacheEntry*)source;
  if (srcrect.GetWidth() == dstrect.GetWidth() && srcrect.GetHeight() == dstrect.GetHeight() &&
      g_ogl_config.bSupportsCopySubImage)
  {
    glCopyImageSubData(srcentry->texture, GL_TEXTURE_2D_ARRAY, 0, srcrect.left, srcrect.top, 0,
                       texture, GL_TEXTURE_2D_ARRAY, 0, dstrect.left, dstrect.top, 0,
                       dstrect.GetWidth(), dstrect.GetHeight(), srcentry->config.layers);
    return;
  }
  else if (!framebuffer)
  {
    glGenFramebuffers(1, &framebuffer);
    FramebufferManager::SetFramebuffer(framebuffer);
    FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D_ARRAY, texture, 0);
  }
  g_renderer->ResetAPIState();
  FramebufferManager::SetFramebuffer(framebuffer);
  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, srcentry->texture);
  g_sampler_cache->BindLinearSampler(9);
  glViewport(dstrect.left, dstrect.top, dstrect.GetWidth(), dstrect.GetHeight());
  s_ColorCopyProgram.Bind();
  glUniform4f(s_ColorCopyPositionUniform, float(srcrect.left), float(srcrect.top),
              float(srcrect.GetWidth()), float(srcrect.GetHeight()));
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  FramebufferManager::SetFramebuffer(0);
  g_renderer->RestoreAPIState();
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
                                     unsigned int expanded_width, unsigned int level)
{
  if (level >= config.levels)
    PanicAlert("Texture only has %d levels, can't update level %d", config.levels, level);
  if (width != std::max(1u, config.width >> level) ||
      height != std::max(1u, config.height >> level))
    PanicAlert("size of level %d must be %dx%d, but %dx%d requested", level,
               std::max(1u, config.width >> level), std::max(1u, config.height >> level), width,
               height);

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

  if (expanded_width != width)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);

  if (g_ogl_config.bSupportsTextureStorage)
  {
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, 0, width, height, 1, GL_RGBA,
                    GL_UNSIGNED_BYTE, temp);
  }
  else
  {
    glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA, width, height, 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, temp);
  }

  if (expanded_width != width)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  TextureCache::SetStage();
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dstPointer, PEControl::PixelFormat srcFormat,
                                                 const EFBRectangle& srcRect, bool scaleByHalf,
                                                 unsigned int cbufid, const float* colmat)
{
  g_renderer->ResetAPIState();  // reset any game specific settings

  // Make sure to resolve anything we need to read from.
  const GLuint read_texture = (srcFormat == PEControl::Z24) ?
                                  FramebufferManager::ResolveAndGetDepthTarget(srcRect) :
                                  FramebufferManager::ResolveAndGetRenderTarget(srcRect);

  FramebufferManager::SetFramebuffer(framebuffer);

  OpenGL_BindAttributelessVAO();

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, read_texture);
  if (scaleByHalf)
    g_sampler_cache->BindLinearSampler(9);
  else
    g_sampler_cache->BindNearestSampler(9);

  glViewport(0, 0, config.width, config.height);

  GLuint uniform_location;
  if (srcFormat == PEControl::Z24)
  {
    s_DepthMatrixProgram.Bind();
    if (s_DepthCbufid != cbufid)
      glUniform4fv(s_DepthMatrixUniform, 5, colmat);
    s_DepthCbufid = cbufid;
    uniform_location = s_DepthCopyPositionUniform;
  }
  else
  {
    s_ColorMatrixProgram.Bind();
    if (s_ColorCbufid != cbufid)
      glUniform4fv(s_ColorMatrixUniform, 7, colmat);
    s_ColorCbufid = cbufid;
    uniform_location = s_ColorMatrixPositionUniform;
  }

  TargetRectangle R = g_renderer->ConvertEFBRectangle(srcRect);
  glUniform4f(uniform_location, static_cast<float>(R.left), static_cast<float>(R.top),
              static_cast<float>(R.right), static_cast<float>(R.bottom));

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  FramebufferManager::SetFramebuffer(0);
  g_renderer->RestoreAPIState();
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row,
                           u32 num_blocks_y, u32 memory_stride, PEControl::PixelFormat srcFormat,
                           const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf)
{
  TextureConverter::EncodeToRamFromTexture(dst, format, native_width, bytes_per_row, num_blocks_y,
                                           memory_stride, srcFormat, isIntensity, scaleByHalf,
                                           srcRect);
}

TextureCache::TextureCache()
{
  CompileShaders();

  s_ActiveTexture = -1;
  for (auto& gtex : s_Textures)
    gtex = -1;

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
  {
    s32 buffer_size = 32 * 1024 * 1024;
    s32 max_buffer_size = 0;

    // The minimum MAX_TEXTURE_BUFFER_SIZE that the spec mandates
    // is 65KB, we are asking for a 1MB buffer here.
    // Make sure to check the maximum size and if it is below 1MB
    // then use the maximum the hardware supports instead.
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &max_buffer_size);
    buffer_size = std::min(buffer_size, max_buffer_size);

    s_palette_stream_buffer = StreamBuffer::Create(GL_TEXTURE_BUFFER, buffer_size);
    glGenTextures(1, &s_palette_resolv_texture);
    glBindTexture(GL_TEXTURE_BUFFER, s_palette_resolv_texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R16UI, s_palette_stream_buffer->m_buffer);

    CreateTextureDecodingResources();
  }
}

TextureCache::~TextureCache()
{
  DeleteShaders();
  DestroyTextureDecodingResources();

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
  {
    s_palette_stream_buffer.reset();
    glDeleteTextures(1, &s_palette_resolv_texture);
  }
}

void TextureCache::DisableStage(unsigned int stage)
{
}

void TextureCache::SetStage()
{
  // -1 is the initial value as we don't know which texture should be bound
  if (s_ActiveTexture != (u32)-1)
    glActiveTexture(GL_TEXTURE0 + s_ActiveTexture);
}

bool TextureCache::CompileShaders()
{
  constexpr const char* color_copy_program = "SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
                                             "in vec3 f_uv0;\n"
                                             "out vec4 ocol0;\n"
                                             "\n"
                                             "void main(){\n"
                                             "	vec4 texcol = texture(samp9, f_uv0);\n"
                                             "	ocol0 = texcol;\n"
                                             "}\n";

  constexpr const char* color_matrix_program =
      "SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
      "uniform vec4 colmat[7];\n"
      "in vec3 f_uv0;\n"
      "out vec4 ocol0;\n"
      "\n"
      "void main(){\n"
      "	vec4 texcol = texture(samp9, f_uv0);\n"
      "	texcol = round(texcol * colmat[5]) * colmat[6];\n"
      "	ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];\n"
      "}\n";

  constexpr const char* depth_matrix_program =
      "SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
      "uniform vec4 colmat[5];\n"
      "in vec3 f_uv0;\n"
      "out vec4 ocol0;\n"
      "\n"
      "void main(){\n"
      "	vec4 texcol = texture(samp9, vec3(f_uv0.xy, %s));\n"
      "	int depth = int(texcol.x * 16777216.0);\n"

      // Convert to Z24 format
      "	ivec4 workspace;\n"
      "	workspace.r = (depth >> 16) & 255;\n"
      "	workspace.g = (depth >> 8) & 255;\n"
      "	workspace.b = depth & 255;\n"

      // Convert to Z4 format
      "	workspace.a = (depth >> 16) & 0xF0;\n"

      // Normalize components to [0.0..1.0]
      "	texcol = vec4(workspace) / 255.0;\n"

      "	ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];\n"
      "}\n";

  constexpr const char* vertex_program =
      "out vec3 %s_uv0;\n"
      "SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
      "uniform vec4 copy_position;\n"  // left, top, right, bottom
      "void main()\n"
      "{\n"
      "	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
      "	%s_uv0 = vec3(mix(copy_position.xy, copy_position.zw, rawpos) / vec2(textureSize(samp9, "
      "0).xy), 0.0);\n"
      "	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
      "}\n";

  const std::string geo_program = g_ActiveConfig.iStereoMode > 0 ?
                                      "layout(triangles) in;\n"
                                      "layout(triangle_strip, max_vertices = 6) out;\n"
                                      "in vec3 v_uv0[3];\n"
                                      "out vec3 f_uv0;\n"
                                      "SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
                                      "void main()\n"
                                      "{\n"
                                      "	int layers = textureSize(samp9, 0).z;\n"
                                      "	for (int layer = 0; layer < layers; ++layer) {\n"
                                      "		for (int i = 0; i < 3; ++i) {\n"
                                      "			f_uv0 = vec3(v_uv0[i].xy, layer);\n"
                                      "			gl_Position = gl_in[i].gl_Position;\n"
                                      "			gl_Layer = layer;\n"
                                      "			EmitVertex();\n"
                                      "		}\n"
                                      "		EndPrimitive();\n"
                                      "	}\n"
                                      "}\n" :
                                      "";

  const char* prefix = geo_program.empty() ? "f" : "v";
  const char* depth_layer = g_ActiveConfig.bStereoEFBMonoDepth ? "0.0" : "f_uv0.z";

  if (!ProgramShaderCache::CompileShader(s_ColorCopyProgram,
                                         StringFromFormat(vertex_program, prefix, prefix),
                                         color_copy_program, geo_program) ||
      !ProgramShaderCache::CompileShader(s_ColorMatrixProgram,
                                         StringFromFormat(vertex_program, prefix, prefix),
                                         color_matrix_program, geo_program) ||
      !ProgramShaderCache::CompileShader(
          s_DepthMatrixProgram, StringFromFormat(vertex_program, prefix, prefix),
          StringFromFormat(depth_matrix_program, depth_layer), geo_program))
  {
    return false;
  }

  s_ColorMatrixUniform = glGetUniformLocation(s_ColorMatrixProgram.glprogid, "colmat");
  s_DepthMatrixUniform = glGetUniformLocation(s_DepthMatrixProgram.glprogid, "colmat");
  s_ColorCbufid = -1;
  s_DepthCbufid = -1;

  s_ColorCopyPositionUniform = glGetUniformLocation(s_ColorCopyProgram.glprogid, "copy_position");
  s_ColorMatrixPositionUniform =
      glGetUniformLocation(s_ColorMatrixProgram.glprogid, "copy_position");
  s_DepthCopyPositionUniform = glGetUniformLocation(s_DepthMatrixProgram.glprogid, "copy_position");

  std::string palette_shader =
      R"GLSL(
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
			ocol0 = DECODE(src);
		}
		)GLSL";

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
  {
    if (!ProgramShaderCache::CompileShader(
            s_palette_pixel_shader[GX_TL_IA8], StringFromFormat(vertex_program, prefix, prefix),
            "#define DECODE DecodePixel_IA8" + palette_shader, geo_program))
    {
      return false;
    }
    s_palette_buffer_offset_uniform[GX_TL_IA8] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "texture_buffer_offset");
    s_palette_multiplier_uniform[GX_TL_IA8] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "multiplier");
    s_palette_copy_position_uniform[GX_TL_IA8] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "copy_position");

    if (!ProgramShaderCache::CompileShader(
            s_palette_pixel_shader[GX_TL_RGB565], StringFromFormat(vertex_program, prefix, prefix),
            "#define DECODE DecodePixel_RGB565" + palette_shader, geo_program))
    {
      return false;
    }
    s_palette_buffer_offset_uniform[GX_TL_RGB565] = glGetUniformLocation(
        s_palette_pixel_shader[GX_TL_RGB565].glprogid, "texture_buffer_offset");
    s_palette_multiplier_uniform[GX_TL_RGB565] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB565].glprogid, "multiplier");
    s_palette_copy_position_uniform[GX_TL_RGB565] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB565].glprogid, "copy_position");

    if (!ProgramShaderCache::CompileShader(
            s_palette_pixel_shader[GX_TL_RGB5A3], StringFromFormat(vertex_program, prefix, prefix),
            "#define DECODE DecodePixel_RGB5A3" + palette_shader, geo_program))
    {
      return false;
    }
    s_palette_buffer_offset_uniform[GX_TL_RGB5A3] = glGetUniformLocation(
        s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "texture_buffer_offset");
    s_palette_multiplier_uniform[GX_TL_RGB5A3] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "multiplier");
    s_palette_copy_position_uniform[GX_TL_RGB5A3] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "copy_position");
  }

  return true;
}

void TextureCache::DeleteShaders()
{
  s_ColorMatrixProgram.Destroy();
  s_DepthMatrixProgram.Destroy();

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
    for (auto& shader : s_palette_pixel_shader)
      shader.Destroy();
}

void TextureCache::ConvertTexture(TCacheEntryBase* _entry, TCacheEntryBase* _unconverted,
                                  void* palette, TlutFormat format)
{
  if (!g_ActiveConfig.backend_info.bSupportsPaletteConversion)
    return;

  g_renderer->ResetAPIState();

  TCacheEntry* entry = (TCacheEntry*)_entry;
  TCacheEntry* unconverted = (TCacheEntry*)_unconverted;

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, unconverted->texture);
  g_sampler_cache->BindNearestSampler(9);

  FramebufferManager::SetFramebuffer(entry->framebuffer);
  glViewport(0, 0, entry->config.width, entry->config.height);
  s_palette_pixel_shader[format].Bind();

  // C14 textures are currently unsupported
  int size = (unconverted->format & 0xf) == GX_TF_I4 ? 32 : 512;
  auto buffer = s_palette_stream_buffer->Map(size);
  memcpy(buffer.first, palette, size);
  s_palette_stream_buffer->Unmap(size);
  glUniform1i(s_palette_buffer_offset_uniform[format], buffer.second / 2);
  glUniform1f(s_palette_multiplier_uniform[format],
              (unconverted->format & 0xf) == 0 ? 15.0f : 255.0f);
  glUniform4f(s_palette_copy_position_uniform[format], 0.0f, 0.0f, (float)unconverted->config.width,
              (float)unconverted->config.height);

  glActiveTexture(GL_TEXTURE10);
  glBindTexture(GL_TEXTURE_BUFFER, s_palette_resolv_texture);
  g_sampler_cache->BindNearestSampler(10);

  OpenGL_BindAttributelessVAO();
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  FramebufferManager::SetFramebuffer(0);
  g_renderer->RestoreAPIState();
}

static const std::string decoding_vertex_shader = R"(
void main()
{
  vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);
  gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);
}
)";

void CreateTextureDecodingResources()
{
  static const GLenum gl_view_types[TextureConversionShader::BUFFER_FORMAT_COUNT] = {
      GL_R8UI,    // BUFFER_FORMAT_R8_UINT
      GL_R16UI,   // BUFFER_FORMAT_R16_UINT
      GL_RG32UI,  // BUFFER_FORMAT_R32G32_UINT
  };

  glGenTextures(TextureConversionShader::BUFFER_FORMAT_COUNT,
                s_texture_decoding_buffer_views.data());
  for (size_t i = 0; i < TextureConversionShader::BUFFER_FORMAT_COUNT; i++)
  {
    glBindTexture(GL_TEXTURE_BUFFER, s_texture_decoding_buffer_views[i]);
    glTexBuffer(GL_TEXTURE_BUFFER, gl_view_types[i], s_palette_stream_buffer->m_buffer);
  }

  s_texture_decoding_use_compute_shader = g_Config.backend_info.bSupportsComputeShaders;
}

void DestroyTextureDecodingResources()
{
  glDeleteTextures(TextureConversionShader::BUFFER_FORMAT_COUNT,
                   s_texture_decoding_buffer_views.data());
  s_texture_decoding_buffer_views.fill(0);
  s_texture_decoding_program_info.clear();

  if (s_texture_decoding_framebuffer != 0)
  {
    glDeleteFramebuffers(1, &s_texture_decoding_framebuffer);
    s_texture_decoding_framebuffer = 0;
  }
}

bool TextureCache::SupportsGPUTextureDecode(TextureFormat format)
{
  const auto iter = s_texture_decoding_program_info.find(format);
  if (iter != s_texture_decoding_program_info.end())
    return iter->second.valid;

  TextureDecodingProgramInfo info;
  info.base_info = TextureConversionShader::GetTextureDecodingShaderInfo(format);
  if (!info.base_info)
  {
    s_texture_decoding_program_info.emplace(format, info);
    return false;
  }

  std::string shader_source = TextureConversionShader::GenerateTextureDecodingShader(
      format, APIType::OpenGL, s_texture_decoding_use_compute_shader);
  if (shader_source.empty())
  {
    s_texture_decoding_program_info.emplace(format, info);
    return false;
  }

  // Try compiling the shader
  if (s_texture_decoding_use_compute_shader)
  {
    if (!ProgramShaderCache::CompileComputeShader(info.program, shader_source))
    {
      s_texture_decoding_program_info.emplace(format, info);
      return false;
    }
  }
  else
  {
    if (!ProgramShaderCache::CompileShader(info.program, decoding_vertex_shader, shader_source))
    {
      s_texture_decoding_program_info.emplace(format, info);
      return false;
    }
  }

  // Get uniforms
  info.uniform_dst_size = glGetUniformLocation(info.program.glprogid, "u_dst_size");
  info.uniform_src_size = glGetUniformLocation(info.program.glprogid, "u_src_size");
  info.uniform_src_offset = glGetUniformLocation(info.program.glprogid, "u_src_offset");
  info.uniform_src_row_stride = glGetUniformLocation(info.program.glprogid, "u_src_row_stride");
  info.valid = true;
  s_texture_decoding_program_info.emplace(format, info);
  return true;
}

void TextureCache::DecodeTexture(TCacheEntryBase* entry, u32 dst_level, const u8* data,
                                 size_t data_size, TextureFormat format, u32 width, u32 height,
                                 u32 aligned_width, u32 aligned_height, const u8* lut,
                                 TlutFormat lut_format)
{
  const auto iter = s_texture_decoding_program_info.find(format);
  if (iter == s_texture_decoding_program_info.end())
    return;

  g_renderer->ResetAPIState();

  if (!s_texture_decoding_use_compute_shader)
  {
    if (s_texture_decoding_framebuffer == 0)
      glGenFramebuffers(1, &s_texture_decoding_framebuffer);

    // Not the most optimal solution, but if we're using fragment shaders it's less
    // than optimal in the first place. We use one framebuffer, but swap out its
    // attachments for each texture that is decoded. Luckily most of the time it's
    // only going to be done once, at least until the texture is re-used.
    FramebufferManager::SetFramebuffer(s_texture_decoding_framebuffer);
    FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D_ARRAY,
                                           static_cast<TCacheEntry*>(entry)->texture, dst_level);

    glViewport(0, 0, width, height);
  }

  // These should already be aligned to block size
  // TODO: We could probably get these from Load()
  u32 block_width = static_cast<u32>(TexDecoder_GetBlockWidthInTexels(format));
  u32 block_height = static_cast<u32>(TexDecoder_GetBlockHeightInTexels(format));
  u32 texel_nibbles = static_cast<u32>(TexDecoder_GetTexelSizeInNibbles(format));
  u32 block_size_in_bytes = (block_width * block_height * texel_nibbles) / 2;
  _assert_((aligned_width % block_width) == 0 && (aligned_height % block_height) == 0);

  // Copy to GPU-visible buffer, aligned to the data type
  auto info = iter->second;
  u32 bytes_per_buffer_elem =
      TextureConversionShader::GetBytesPerBufferElement(info.base_info->buffer_format);
  auto buffer = s_palette_stream_buffer->Map(static_cast<u32>(data_size), bytes_per_buffer_elem);
  memcpy(buffer.first, data, data_size);
  s_palette_stream_buffer->Unmap(static_cast<u32>(data_size));

  info.program.Bind();

  // Calculate stride in buffer elements
  u32 row_stride = block_size_in_bytes * (aligned_width / block_width);
  u32 row_stride_in_buffer_elements = row_stride / bytes_per_buffer_elem;
  u32 offset_in_buffer_elements = buffer.second / bytes_per_buffer_elem;
  if (info.uniform_dst_size >= 0)
    glUniform2ui(info.uniform_dst_size, width, height);
  if (info.uniform_src_size >= 0)
    glUniform2ui(info.uniform_src_size, aligned_width, aligned_height);
  if (info.uniform_src_offset >= 0)
    glUniform1ui(info.uniform_src_offset, offset_in_buffer_elements);
  if (info.uniform_src_row_stride >= 0)
    glUniform1ui(info.uniform_src_row_stride, row_stride_in_buffer_elements);

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_BUFFER, s_texture_decoding_buffer_views[info.base_info->buffer_format]);
  g_sampler_cache->BindNearestSampler(9);

  if (!s_texture_decoding_use_compute_shader)
  {
    OpenGL_BindAttributelessVAO();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    FramebufferManager::SetFramebuffer(0);
  }
  else
  {
    auto dispatch_groups = TextureConversionShader::GetDispatchCount(info.base_info, width, height);
    glBindImageTexture(0, static_cast<TCacheEntry*>(entry)->texture, dst_level, GL_TRUE, 0,
                       GL_WRITE_ONLY, GL_RGBA8);
    glDispatchCompute(dispatch_groups.first, dispatch_groups.second, 1);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
  }

  g_renderer->RestoreAPIState();
  TextureCache::SetStage();
}
}
