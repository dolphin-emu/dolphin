// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Fast image conversion using OpenGL shaders.

#include <string>

#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/TextureCache.h"
#include "VideoBackends/OGL/TextureConverter.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
namespace TextureConverter
{
using OGL::TextureCache;

static GLuint s_texConvFrameBuffer[2] = {0, 0};
static GLuint s_srcTexture = 0;  // for decoding from RAM
static GLuint s_dstTexture = 0;  // for encoding to RAM

const int renderBufferWidth = EFB_WIDTH * 4;
const int renderBufferHeight = 1024;

// Not all slots are taken - but who cares.
const u32 NUM_ENCODING_PROGRAMS = 64;
static SHADER s_encodingPrograms[NUM_ENCODING_PROGRAMS];
static int s_encodingUniforms[NUM_ENCODING_PROGRAMS];

static GLuint s_PBO = 0;  // for readback with different strides

static SHADER& GetOrCreateEncodingShader(u32 format)
{
  if (format >= NUM_ENCODING_PROGRAMS)
  {
    PanicAlert("Unknown texture copy format: 0x%x\n", format);
    return s_encodingPrograms[0];
  }

  if (s_encodingPrograms[format].glprogid == 0)
  {
    const char* shader = TextureConversionShader::GenerateEncodingShader(format, API_OPENGL);

#if defined(_DEBUG) || defined(DEBUGFAST)
    if (g_ActiveConfig.iLog & CONF_SAVESHADERS && shader)
    {
      static int counter = 0;
      std::string filename =
          StringFromFormat("%senc_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);

      SaveData(filename, shader);
    }
#endif

    const char* VProgram = "void main()\n"
                           "{\n"
                           "	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
                           "	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
                           "}\n";

    ProgramShaderCache::CompileShader(s_encodingPrograms[format], VProgram, shader);

    s_encodingUniforms[format] =
        glGetUniformLocation(s_encodingPrograms[format].glprogid, "position");
  }
  return s_encodingPrograms[format];
}

void Init()
{
  glGenFramebuffers(2, s_texConvFrameBuffer);

  glActiveTexture(GL_TEXTURE9);
  glGenTextures(1, &s_srcTexture);
  glBindTexture(GL_TEXTURE_2D, s_srcTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

  glGenTextures(1, &s_dstTexture);
  glBindTexture(GL_TEXTURE_2D, s_dstTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, renderBufferWidth, renderBufferHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  FramebufferManager::SetFramebuffer(s_texConvFrameBuffer[0]);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_dstTexture, 0);
  FramebufferManager::SetFramebuffer(0);

  glGenBuffers(1, &s_PBO);
}

void Shutdown()
{
  glDeleteTextures(1, &s_srcTexture);
  glDeleteTextures(1, &s_dstTexture);
  glDeleteBuffers(1, &s_PBO);
  glDeleteFramebuffers(2, s_texConvFrameBuffer);

  for (auto& program : s_encodingPrograms)
    program.Destroy();

  s_srcTexture = 0;
  s_dstTexture = 0;
  s_PBO = 0;
  s_texConvFrameBuffer[0] = 0;
  s_texConvFrameBuffer[1] = 0;
}

// dst_line_size, writeStride in bytes

static void EncodeToRamUsingShader(GLuint srcTexture, u8* destAddr, u32 dst_line_size,
                                   u32 dstHeight, u32 writeStride, bool linearFilter)
{
  // switch to texture converter frame buffer
  // attach render buffer as color destination
  FramebufferManager::SetFramebuffer(s_texConvFrameBuffer[0]);

  OpenGL_BindAttributelessVAO();

  // set source texture
  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, srcTexture);

  if (linearFilter)
    g_sampler_cache->BindLinearSampler(9);
  else
    g_sampler_cache->BindNearestSampler(9);

  glViewport(0, 0, (GLsizei)(dst_line_size / 4), (GLsizei)dstHeight);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  int dstSize = dst_line_size * dstHeight;

  if ((writeStride != dst_line_size) && (dstHeight > 1))
  {
    // writing to a texture of a different size
    // also copy more then one block line, so the different strides matters
    // copy into one pbo first, map this buffer, and then memcpy into GC memory
    // in this way, we only have one vram->ram transfer, but maybe a bigger
    // CPU overhead because of the pbo
    glBindBuffer(GL_PIXEL_PACK_BUFFER, s_PBO);
    glBufferData(GL_PIXEL_PACK_BUFFER, dstSize, nullptr, GL_STREAM_READ);
    glReadPixels(0, 0, (GLsizei)(dst_line_size / 4), (GLsizei)dstHeight, GL_BGRA, GL_UNSIGNED_BYTE,
                 nullptr);
    u8* pbo = (u8*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, dstSize, GL_MAP_READ_BIT);

    for (size_t i = 0; i < dstHeight; ++i)
    {
      memcpy(destAddr, pbo, dst_line_size);
      pbo += dst_line_size;
      destAddr += writeStride;
    }

    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  }
  else
  {
    glReadPixels(0, 0, (GLsizei)(dst_line_size / 4), (GLsizei)dstHeight, GL_BGRA, GL_UNSIGNED_BYTE,
                 destAddr);
  }
}

void EncodeToRamFromTexture(u8* dest_ptr, u32 format, u32 native_width, u32 bytes_per_row,
                            u32 num_blocks_y, u32 memory_stride, PEControl::PixelFormat srcFormat,
                            bool bIsIntensityFmt, int bScaleByHalf, const EFBRectangle& source)
{
  g_renderer->ResetAPIState();

  SHADER& texconv_shader = GetOrCreateEncodingShader(format);

  texconv_shader.Bind();
  glUniform4i(s_encodingUniforms[format], source.left, source.top, native_width,
              bScaleByHalf ? 2 : 1);

  const GLuint read_texture = (srcFormat == PEControl::Z24) ?
                                  FramebufferManager::ResolveAndGetDepthTarget(source) :
                                  FramebufferManager::ResolveAndGetRenderTarget(source);

  EncodeToRamUsingShader(read_texture, dest_ptr, bytes_per_row, num_blocks_y, memory_stride,
                         bScaleByHalf > 0 && srcFormat != PEControl::Z24);

  FramebufferManager::SetFramebuffer(0);
  g_renderer->RestoreAPIState();
}

}  // namespace

}  // namespace OGL
