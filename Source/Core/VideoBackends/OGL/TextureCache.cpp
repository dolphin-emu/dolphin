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

#include "VideoBackends/OGL/AbstractTexture.h"
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
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
SHADER s_ColorCopyProgram;
SHADER s_ColorMatrixProgram;
SHADER s_DepthMatrixProgram;
GLuint s_ColorMatrixUniform;
GLuint s_DepthMatrixUniform;
GLuint s_ColorCopyPositionUniform;
GLuint s_ColorMatrixPositionUniform;
GLuint s_DepthCopyPositionUniform;
u32 s_ColorCbufid;
u32 s_DepthCbufid;

static SHADER s_palette_pixel_shader[3];
static std::unique_ptr<StreamBuffer> s_palette_stream_buffer;
static GLuint s_palette_resolv_texture;
static GLuint s_palette_buffer_offset_uniform[3];
static GLuint s_palette_multiplier_uniform[3];
static GLuint s_palette_copy_position_uniform[3];

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

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
  {
    s32 buffer_size = 1024 * 1024;
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
  }
}

TextureCache::~TextureCache()
{
  DeleteShaders();

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
  {
    s_palette_stream_buffer.reset();
    glDeleteTextures(1, &s_palette_resolv_texture);
  }
}

void TextureCache::CompileShaders()
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

  ProgramShaderCache::CompileShader(s_ColorCopyProgram,
                                    StringFromFormat(vertex_program, prefix, prefix).c_str(),
                                    color_copy_program, geo_program);
  ProgramShaderCache::CompileShader(s_ColorMatrixProgram,
                                    StringFromFormat(vertex_program, prefix, prefix).c_str(),
                                    color_matrix_program, geo_program);
  ProgramShaderCache::CompileShader(
      s_DepthMatrixProgram, StringFromFormat(vertex_program, prefix, prefix).c_str(),
      StringFromFormat(depth_matrix_program, depth_layer).c_str(), geo_program);

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
    ProgramShaderCache::CompileShader(
        s_palette_pixel_shader[GX_TL_IA8], StringFromFormat(vertex_program, prefix, prefix),
        "#define DECODE DecodePixel_IA8" + palette_shader, geo_program);
    s_palette_buffer_offset_uniform[GX_TL_IA8] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "texture_buffer_offset");
    s_palette_multiplier_uniform[GX_TL_IA8] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "multiplier");
    s_palette_copy_position_uniform[GX_TL_IA8] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_IA8].glprogid, "copy_position");

    ProgramShaderCache::CompileShader(
        s_palette_pixel_shader[GX_TL_RGB565], StringFromFormat(vertex_program, prefix, prefix),
        "#define DECODE DecodePixel_RGB565" + palette_shader, geo_program);
    s_palette_buffer_offset_uniform[GX_TL_RGB565] = glGetUniformLocation(
        s_palette_pixel_shader[GX_TL_RGB565].glprogid, "texture_buffer_offset");
    s_palette_multiplier_uniform[GX_TL_RGB565] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB565].glprogid, "multiplier");
    s_palette_copy_position_uniform[GX_TL_RGB565] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB565].glprogid, "copy_position");

    ProgramShaderCache::CompileShader(
        s_palette_pixel_shader[GX_TL_RGB5A3], StringFromFormat(vertex_program, prefix, prefix),
        "#define DECODE DecodePixel_RGB5A3" + palette_shader, geo_program);
    s_palette_buffer_offset_uniform[GX_TL_RGB5A3] = glGetUniformLocation(
        s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "texture_buffer_offset");
    s_palette_multiplier_uniform[GX_TL_RGB5A3] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "multiplier");
    s_palette_copy_position_uniform[GX_TL_RGB5A3] =
        glGetUniformLocation(s_palette_pixel_shader[GX_TL_RGB5A3].glprogid, "copy_position");
  }
}

void TextureCache::DeleteShaders()
{
  s_ColorMatrixProgram.Destroy();
  s_DepthMatrixProgram.Destroy();

  if (g_ActiveConfig.backend_info.bSupportsPaletteConversion)
    for (auto& shader : s_palette_pixel_shader)
      shader.Destroy();
}

std::unique_ptr<AbstractTextureBase>
TextureCache::CreateTexture(const AbstractTextureBase::TextureConfig& config)
{
  return std::make_unique<AbstractTexture>(config);
}

void TextureCache::ConvertTexture(TCacheEntry* dest, TCacheEntry* source, void* palette,
                                  TlutFormat format)
{
  if (!g_ActiveConfig.backend_info.bSupportsPaletteConversion)
    return;

  g_renderer->ResetAPIState();

  AbstractTexture* dest_tex = static_cast<AbstractTexture*>(dest->texture.get());
  AbstractTexture* source_tex = static_cast<AbstractTexture*>(source->texture.get());

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, source_tex->texture);
  g_sampler_cache->BindNearestSampler(9);

  FramebufferManager::SetFramebuffer(dest_tex->framebuffer);
  glViewport(0, 0, dest->width(), dest->height());
  s_palette_pixel_shader[format].Bind();

  // C14 textures are currently unsupported
  int size = (source->format & 0xf) == GX_TF_I4 ? 32 : 512;
  auto buffer = s_palette_stream_buffer->Map(size);
  memcpy(buffer.first, palette, size);
  s_palette_stream_buffer->Unmap(size);
  glUniform1i(s_palette_buffer_offset_uniform[format], buffer.second / 2);
  glUniform1f(s_palette_multiplier_uniform[format], (source->format & 0xf) == 0 ? 15.0f : 255.0f);
  glUniform4f(s_palette_copy_position_uniform[format], 0.0f, 0.0f, (float)source->width(),
              (float)source->height());

  glActiveTexture(GL_TEXTURE10);
  glBindTexture(GL_TEXTURE_BUFFER, s_palette_resolv_texture);
  g_sampler_cache->BindNearestSampler(10);

  OpenGL_BindAttributelessVAO();
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  FramebufferManager::SetFramebuffer(0);
  g_renderer->RestoreAPIState();
}
}
