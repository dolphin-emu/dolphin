// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/FramebufferManager.h"

#include <memory>
#include <vector>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/TextureConverter.h"
#include "VideoBackends/OGL/VertexManager.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoBackendBase.h"

constexpr const char* GLSL_REINTERPRET_PIXELFMT_VS = R"GLSL(
flat out int layer;
void main(void) {
  layer = 0;
  vec2 rawpos = vec2(gl_VertexID & 1, gl_VertexID & 2);
  gl_Position = vec4(rawpos* 2.0 - 1.0, 0.0, 1.0);
})GLSL";

constexpr const char* GLSL_SHADER_FS = R"GLSL(
#define MULTILAYER %d
#define MSAA %d

#if MSAA

#if MULTILAYER
SAMPLER_BINDING(9) uniform sampler2DMSArray samp9;
#else
SAMPLER_BINDING(9) uniform sampler2DMS samp9;
#endif

#else
SAMPLER_BINDING(9) uniform sampler2DArray samp9;
#endif

vec4 sampleEFB(ivec3 pos) {
#if MSAA

#if MULTILAYER
  return texelFetch(samp9, pos, gl_SampleID);
#else
  return texelFetch(samp9, pos.xy, gl_SampleID);
#endif

#else
  return texelFetch(samp9, pos, 0);
#endif
})GLSL";

constexpr const char* GLSL_SAMPLE_EFB_FS = R"GLSL(
#define MULTILAYER %d

#if MULTILAYER
SAMPLER_BINDING(9) uniform sampler2DMSArray samp9;
#else
SAMPLER_BINDING(9) uniform sampler2DMS samp9;
#endif
vec4 sampleEFB(ivec3 pos) {
  vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
  for (int i = 0; i < %d; i++)
#if MULTILAYER
    color += texelFetch(samp9, pos, i);
#else
    color += texelFetch(samp9, pos.xy, i);
#endif

  return color / %d;
})GLSL";

constexpr const char* GLSL_RGBA6_TO_RGB8_FS = R"GLSL(
flat in int layer;
out vec4 ocol0;
void main() {
  ivec4 src6 = ivec4(round(sampleEFB(ivec3(gl_FragCoord.xy, layer)) * 63.f));
  ivec4 dst8;

  dst8.r = (src6.r << 2) | (src6.g >> 4);
  dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);
  dst8.b = ((src6.b & 0x3) << 6) | src6.a;
  dst8.a = 255;

  ocol0 = float4(dst8) / 255.f;
})GLSL";

constexpr const char* GLSL_RGB8_TO_RGBA6_FS = R"GLSL(
flat in int layer;
out vec4 ocol0;
void main() {
  ivec4 src8 = ivec4(round(sampleEFB(ivec3(gl_FragCoord.xy, layer)) * 255.f));
  ivec4 dst6;

  dst6.r = src8.r >> 2;
  dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);
  dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);
  dst6.a = src8.b & 0x3F;
  ocol0 = float4(dst6) / 63.f;
})GLSL";

constexpr const char* GLSL_GS = R"GLSL(
layout(triangles) in;
layout(triangle_strip, max_vertices = %d) out;
flat out int layer;
void main() {
  for (int j = 0; j < %d; ++j) {
    for (int i = 0; i < 3; ++i) {
      layer = j;
      gl_Layer = j;
      gl_Position = gl_in[i].gl_Position;
      EmitVertex();
    }
    EndPrimitive();
  }
})GLSL";

constexpr const char* GLSL_EFB_POKE_VERTEX_VS = R"GLSL(
in vec2 rawpos;
in vec4 rawcolor0; // color
in int rawcolor1;  // depth
out vec4 v_c;
out float v_z;
void main(void) {
  gl_Position = vec4(((rawpos + 0.5) / vec2(640.0, 528.0) * 2.0 - 1.0) * vec2(1.0, -1.0), 0.0, 1.0);
  gl_PointSize = %d.0 / 640.0;

  v_c = rawcolor0.bgra;
  v_z = float(rawcolor1 & 0xFFFFFF) / 16777216.0;
})GLSL";

constexpr const char* GLSL_EFB_POKE_PIXEL_FS = R"GLSL(
in vec4 %s_c;
in float %s_z;
out vec4 ocol0;
void main(void) {
  ocol0 = %s_c;
  gl_FragDepth = %s_z;
})GLSL";

constexpr const char* GLSL_EFB_POKE_GEOMETRY_GS = R"GLSL(
layout(points) in;
layout(points, max_vertices = %d) out;
in vec4 v_c[1];
in float v_z[1];
out vec4 g_c;
out float g_z;
void main() {
  for (int j = 0; j < %d; ++j) {
    gl_Layer = j;
    gl_Position = gl_in[0].gl_Position;
    gl_PointSize = %d.0 / 640.0;
    g_c = v_c[0];
    g_z = v_z[0];

    EmitVertex();
    EndPrimitive();
  }
})GLSL";

namespace OGL
{
int FramebufferManager::m_targetWidth;
int FramebufferManager::m_targetHeight;
int FramebufferManager::m_msaaSamples;
bool FramebufferManager::m_enable_stencil_buffer;

GLenum FramebufferManager::m_textureType;
std::vector<GLuint> FramebufferManager::m_efbFramebuffer;
GLuint FramebufferManager::m_efbColor;
GLuint FramebufferManager::m_efbDepth;
GLuint FramebufferManager::m_efbColorSwap;  // for hot swap when reinterpreting EFB pixel formats

// Only used in MSAA mode.
std::vector<GLuint> FramebufferManager::m_resolvedFramebuffer;
GLuint FramebufferManager::m_resolvedColorTexture;
GLuint FramebufferManager::m_resolvedDepthTexture;

// reinterpret pixel format
SHADER FramebufferManager::m_pixel_format_shaders[2];

// EFB pokes
GLuint FramebufferManager::m_EfbPokes_VBO;
GLuint FramebufferManager::m_EfbPokes_VAO;
SHADER FramebufferManager::m_EfbPokes;

GLuint FramebufferManager::CreateTexture(GLenum texture_type, GLenum internal_format,
                                         GLenum pixel_format, GLenum data_type)
{
  GLuint texture;
  glActiveTexture(GL_TEXTURE9);
  glGenTextures(1, &texture);
  glBindTexture(texture_type, texture);
  if (texture_type == GL_TEXTURE_2D_ARRAY)
  {
    glTexParameteri(texture_type, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage3D(texture_type, 0, internal_format, m_targetWidth, m_targetHeight, m_EFBLayers, 0,
                 pixel_format, data_type, nullptr);
  }
  else if (texture_type == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
  {
    if (g_ogl_config.bSupports3DTextureStorageMultisample)
      glTexStorage3DMultisample(texture_type, m_msaaSamples, internal_format, m_targetWidth,
                                m_targetHeight, m_EFBLayers, false);
    else
      glTexImage3DMultisample(texture_type, m_msaaSamples, internal_format, m_targetWidth,
                              m_targetHeight, m_EFBLayers, false);
  }
  else if (texture_type == GL_TEXTURE_2D_MULTISAMPLE)
  {
    if (g_ogl_config.bSupports2DTextureStorageMultisample)
      glTexStorage2DMultisample(texture_type, m_msaaSamples, internal_format, m_targetWidth,
                                m_targetHeight, false);
    else
      glTexImage2DMultisample(texture_type, m_msaaSamples, internal_format, m_targetWidth,
                              m_targetHeight, false);
  }
  else
  {
    PanicAlert("Unhandled texture type %d", texture_type);
  }
  glBindTexture(texture_type, 0);
  return texture;
}

void FramebufferManager::BindLayeredTexture(GLuint texture, const std::vector<GLuint>& framebuffers,
                                            GLenum attachment, GLenum texture_type)
{
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
  FramebufferTexture(GL_FRAMEBUFFER, attachment, texture_type, texture, 0);
  // Bind all the other layers as separate FBOs for blitting.
  for (unsigned int i = 1; i < m_EFBLayers; i++)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[i]);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture, 0, i);
  }
}

bool FramebufferManager::HasStencilBuffer()
{
  return m_enable_stencil_buffer;
}

FramebufferManager::FramebufferManager(int targetWidth, int targetHeight, int msaaSamples,
                                       bool enable_stencil_buffer)
{
  m_efbColor = 0;
  m_efbDepth = 0;
  m_efbColorSwap = 0;
  m_resolvedColorTexture = 0;
  m_resolvedDepthTexture = 0;

  m_targetWidth = targetWidth;
  m_targetHeight = targetHeight;
  m_msaaSamples = msaaSamples;
  m_enable_stencil_buffer = enable_stencil_buffer;

  // The EFB can be set to different pixel formats by the game through the
  // BPMEM_ZCOMPARE register (which should probably have a different name).
  // They are:
  // - 24-bit RGB (8-bit components) with 24-bit Z
  // - 24-bit RGBA (6-bit components) with 24-bit Z
  // - Multisampled 16-bit RGB (5-6-5 format) with 16-bit Z
  // We only use one EFB format here: 32-bit ARGB with 24-bit Z.
  // Multisampling depends on user settings.
  // The distinction becomes important for certain operations, i.e. the
  // alpha channel should be ignored if the EFB does not have one.

  glActiveTexture(GL_TEXTURE9);

  m_EFBLayers = (g_ActiveConfig.stereo_mode != StereoMode::Off) ? 2 : 1;
  m_efbFramebuffer.resize(m_EFBLayers);
  m_resolvedFramebuffer.resize(m_EFBLayers);

  GLenum depth_internal_format = GL_DEPTH_COMPONENT32F;
  GLenum depth_pixel_format = GL_DEPTH_COMPONENT;
  GLenum depth_data_type = GL_FLOAT;
  if (m_enable_stencil_buffer)
  {
    depth_internal_format = GL_DEPTH32F_STENCIL8;
    depth_pixel_format = GL_DEPTH_STENCIL;
    depth_data_type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
  }

  const bool multilayer = m_EFBLayers > 1;

  if (m_msaaSamples <= 1)
  {
    m_textureType = GL_TEXTURE_2D_ARRAY;
  }
  else
  {
    // Only use a layered multisample texture if needed. Some drivers
    // slow down significantly with single-layered multisample textures.
    m_textureType = multilayer ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_MULTISAMPLE;

    // Although we are able to access the multisampled texture directly, we don't do it
    // everywhere. The old way is to "resolve" this multisampled texture by copying it into a
    // non-sampled texture. This would lead to an unneeded copy of the EFB, so we are going to
    // avoid it. But as this job isn't done right now, we do need that texture for resolving:
    GLenum resolvedType = GL_TEXTURE_2D_ARRAY;

    m_resolvedColorTexture = CreateTexture(resolvedType, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    m_resolvedDepthTexture =
        CreateTexture(resolvedType, depth_internal_format, depth_pixel_format, depth_data_type);

    // Bind resolved textures to resolved framebuffer.
    glGenFramebuffers(m_EFBLayers, m_resolvedFramebuffer.data());
    BindLayeredTexture(m_resolvedColorTexture, m_resolvedFramebuffer, GL_COLOR_ATTACHMENT0,
                       resolvedType);
    BindLayeredTexture(m_resolvedDepthTexture, m_resolvedFramebuffer, GL_DEPTH_ATTACHMENT,
                       resolvedType);
    if (m_enable_stencil_buffer)
      BindLayeredTexture(m_resolvedDepthTexture, m_resolvedFramebuffer, GL_STENCIL_ATTACHMENT,
                         resolvedType);
  }

  m_efbColor = CreateTexture(m_textureType, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
  m_efbDepth =
      CreateTexture(m_textureType, depth_internal_format, depth_pixel_format, depth_data_type);
  m_efbColorSwap = CreateTexture(m_textureType, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

  // Bind target textures to EFB framebuffer.
  glGenFramebuffers(m_EFBLayers, m_efbFramebuffer.data());
  BindLayeredTexture(m_efbColor, m_efbFramebuffer, GL_COLOR_ATTACHMENT0, m_textureType);
  BindLayeredTexture(m_efbDepth, m_efbFramebuffer, GL_DEPTH_ATTACHMENT, m_textureType);
  if (m_enable_stencil_buffer)
    BindLayeredTexture(m_efbDepth, m_efbFramebuffer, GL_STENCIL_ATTACHMENT, m_textureType);

  // EFB framebuffer is currently bound, make sure to clear it before use.
  glViewport(0, 0, m_targetWidth, m_targetHeight);
  glScissor(0, 0, m_targetWidth, m_targetHeight);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClearDepthf(1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (m_enable_stencil_buffer)
  {
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
  }

  // reinterpret pixel format
  std::string vs = GLSL_REINTERPRET_PIXELFMT_VS;

  // The way to sample the EFB is based on the on the current configuration.
  // As we use the same sampling way for both interpreting shaders, the sampling
  // shader are generated first:
  std::string sampler;

  if (m_msaaSamples <= 1)
  {
    // non-msaa, so just fetch the pixel
    sampler = StringFromFormat(GLSL_SHADER_FS, multilayer, false);
  }
  else if (g_ActiveConfig.backend_info.bSupportsSSAA)
  {
    // msaa + sample shading available, so just fetch the sample
    // This will lead to sample shading, but it's the only way to not loose
    // the values of each sample.
    sampler = StringFromFormat(GLSL_SHADER_FS, multilayer, true);
  }
  else
  {
    // msaa without sample shading: calculate the mean value of the pixel
    sampler = StringFromFormat(GLSL_SAMPLE_EFB_FS, multilayer, m_msaaSamples, m_msaaSamples);
  }

  std::string ps_rgba6_to_rgb8 = sampler + GLSL_RGBA6_TO_RGB8_FS;

  std::string ps_rgb8_to_rgba6 = sampler + GLSL_RGB8_TO_RGBA6_FS;

  std::string gs = StringFromFormat(GLSL_GS, m_EFBLayers * 3, m_EFBLayers);

  ProgramShaderCache::CompileShader(m_pixel_format_shaders[0], vs, ps_rgb8_to_rgba6,
                                    multilayer ? gs : "");
  ProgramShaderCache::CompileShader(m_pixel_format_shaders[1], vs, ps_rgba6_to_rgb8,
                                    multilayer ? gs : "");

  const auto prefix = multilayer ? "g" : "v";

  ProgramShaderCache::CompileShader(
      m_EfbPokes, StringFromFormat(GLSL_EFB_POKE_VERTEX_VS, m_targetWidth),

      StringFromFormat(GLSL_EFB_POKE_PIXEL_FS, prefix, prefix, prefix, prefix),

      multilayer ?
          StringFromFormat(GLSL_EFB_POKE_GEOMETRY_GS, m_EFBLayers, m_EFBLayers, m_targetWidth) :
          "");
  glGenBuffers(1, &m_EfbPokes_VBO);
  glGenVertexArrays(1, &m_EfbPokes_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, m_EfbPokes_VBO);
  glBindVertexArray(m_EfbPokes_VAO);
  glEnableVertexAttribArray(SHADER_POSITION_ATTRIB);
  glVertexAttribPointer(SHADER_POSITION_ATTRIB, 2, GL_UNSIGNED_SHORT, 0, sizeof(EfbPokeData),
                        (void*)offsetof(EfbPokeData, x));
  glEnableVertexAttribArray(SHADER_COLOR0_ATTRIB);
  glVertexAttribPointer(SHADER_COLOR0_ATTRIB, 4, GL_UNSIGNED_BYTE, 1, sizeof(EfbPokeData),
                        (void*)offsetof(EfbPokeData, data));
  glEnableVertexAttribArray(SHADER_COLOR1_ATTRIB);
  glVertexAttribIPointer(SHADER_COLOR1_ATTRIB, 1, GL_INT, sizeof(EfbPokeData),
                         (void*)offsetof(EfbPokeData, data));
  glBindBuffer(GL_ARRAY_BUFFER,
               static_cast<VertexManager*>(g_vertex_manager.get())->GetVertexBufferHandle());

  if (!static_cast<Renderer*>(g_renderer.get())->IsGLES())
    glEnable(GL_PROGRAM_POINT_SIZE);
}

FramebufferManager::~FramebufferManager()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  GLuint glObj[3];

  // Note: OpenGL deletion functions silently ignore parameters of "0".

  glDeleteFramebuffers(m_EFBLayers, m_efbFramebuffer.data());
  glDeleteFramebuffers(m_EFBLayers, m_resolvedFramebuffer.data());

  // Required, as these are static class members
  m_efbFramebuffer.clear();
  m_resolvedFramebuffer.clear();

  glObj[0] = m_resolvedColorTexture;
  glObj[1] = m_resolvedDepthTexture;
  glDeleteTextures(2, glObj);
  m_resolvedColorTexture = 0;
  m_resolvedDepthTexture = 0;

  glObj[0] = m_efbColor;
  glObj[1] = m_efbDepth;
  glObj[2] = m_efbColorSwap;
  glDeleteTextures(3, glObj);
  m_efbColor = 0;
  m_efbDepth = 0;
  m_efbColorSwap = 0;

  // reinterpret pixel format
  m_pixel_format_shaders[0].Destroy();
  m_pixel_format_shaders[1].Destroy();

  // EFB pokes
  glDeleteBuffers(1, &m_EfbPokes_VBO);
  glDeleteVertexArrays(1, &m_EfbPokes_VAO);
  m_EfbPokes_VBO = 0;
  m_EfbPokes_VAO = 0;
  m_EfbPokes.Destroy();
}

GLuint FramebufferManager::GetEFBColorTexture(const EFBRectangle& sourceRc)
{
  if (m_msaaSamples <= 1)
  {
    return m_efbColor;
  }
  else
  {
    // Transfer the EFB to a resolved texture. EXT_framebuffer_blit is
    // required.

    TargetRectangle targetRc = g_renderer->ConvertEFBRectangle(sourceRc);
    targetRc.ClampUL(0, 0, m_targetWidth, m_targetHeight);

    // Resolve.
    for (unsigned int i = 0; i < m_EFBLayers; i++)
    {
      glBindFramebuffer(GL_READ_FRAMEBUFFER, m_efbFramebuffer[i]);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_resolvedFramebuffer[i]);
      glBlitFramebuffer(targetRc.left, targetRc.top, targetRc.right, targetRc.bottom, targetRc.left,
                        targetRc.top, targetRc.right, targetRc.bottom, GL_COLOR_BUFFER_BIT,
                        GL_NEAREST);
    }

    // Return to EFB.
    glBindFramebuffer(GL_FRAMEBUFFER, m_efbFramebuffer[0]);

    return m_resolvedColorTexture;
  }
}

GLuint FramebufferManager::GetEFBDepthTexture(const EFBRectangle& sourceRc)
{
  if (m_msaaSamples <= 1)
  {
    return m_efbDepth;
  }
  else
  {
    // Transfer the EFB to a resolved texture.

    TargetRectangle targetRc = g_renderer->ConvertEFBRectangle(sourceRc);
    targetRc.ClampUL(0, 0, m_targetWidth, m_targetHeight);

    // Resolve.
    for (unsigned int i = 0; i < m_EFBLayers; i++)
    {
      glBindFramebuffer(GL_READ_FRAMEBUFFER, m_efbFramebuffer[i]);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_resolvedFramebuffer[i]);
      glBlitFramebuffer(targetRc.left, targetRc.top, targetRc.right, targetRc.bottom, targetRc.left,
                        targetRc.top, targetRc.right, targetRc.bottom, GL_DEPTH_BUFFER_BIT,
                        GL_NEAREST);
    }

    // Return to EFB.
    glBindFramebuffer(GL_FRAMEBUFFER, m_efbFramebuffer[0]);

    return m_resolvedDepthTexture;
  }
}

void FramebufferManager::ResolveEFBStencilTexture()
{
  if (m_msaaSamples <= 1)
    return;

  // Resolve.
  for (unsigned int i = 0; i < m_EFBLayers; i++)
  {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_efbFramebuffer[i]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_resolvedFramebuffer[i]);
    glBlitFramebuffer(0, 0, m_targetWidth, m_targetHeight, 0, 0, m_targetWidth, m_targetHeight,
                      GL_STENCIL_BUFFER_BIT, GL_NEAREST);
  }

  // Return to EFB.
  glBindFramebuffer(GL_FRAMEBUFFER, m_efbFramebuffer[0]);
}

GLuint FramebufferManager::GetResolvedFramebuffer()
{
  if (m_msaaSamples <= 1)
    return m_efbFramebuffer[0];
  return m_resolvedFramebuffer[0];
}

void FramebufferManager::SetFramebuffer(GLuint fb)
{
  glBindFramebuffer(GL_FRAMEBUFFER, fb != 0 ? fb : GetEFBFramebuffer());
}

void FramebufferManager::FramebufferTexture(GLenum target, GLenum attachment, GLenum textarget,
                                            GLuint texture, GLint level)
{
  if (textarget == GL_TEXTURE_2D_ARRAY || textarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
  {
    if (m_EFBLayers > 1)
      glFramebufferTexture(target, attachment, texture, level);
    else
      glFramebufferTextureLayer(target, attachment, texture, level, 0);
  }
  else
  {
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
  }
}

// Apply AA if enabled
GLuint FramebufferManager::ResolveAndGetRenderTarget(const EFBRectangle& source_rect)
{
  return GetEFBColorTexture(source_rect);
}

GLuint FramebufferManager::ResolveAndGetDepthTarget(const EFBRectangle& source_rect)
{
  return GetEFBDepthTexture(source_rect);
}

void FramebufferManager::ReinterpretPixelData(unsigned int convtype)
{
  g_renderer->ResetAPIState();

  GLuint src_texture = 0;

  // We aren't allowed to render and sample the same texture in one draw call,
  // so we have to create a new texture and overwrite it completely.
  // To not allocate one big texture every time, we've allocated two on
  // initialization and just swap them here:
  src_texture = m_efbColor;
  m_efbColor = m_efbColorSwap;
  m_efbColorSwap = src_texture;
  FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_textureType, m_efbColor, 0);

  glViewport(0, 0, m_targetWidth, m_targetHeight);
  glActiveTexture(GL_TEXTURE9);
  glBindTexture(m_textureType, src_texture);
  g_sampler_cache->BindNearestSampler(9);

  m_pixel_format_shaders[convtype ? 1 : 0].Bind();
  ProgramShaderCache::BindVertexFormat(nullptr);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindTexture(m_textureType, 0);

  g_renderer->RestoreAPIState();
}

void FramebufferManager::PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
  g_renderer->ResetAPIState();

  if (type == EFBAccessType::PokeZ)
  {
    glDepthMask(GL_TRUE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
  }

  glBindVertexArray(m_EfbPokes_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, m_EfbPokes_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(EfbPokeData) * num_points, points, GL_STREAM_DRAW);
  m_EfbPokes.Bind();
  glViewport(0, 0, m_targetWidth, m_targetHeight);
  glDrawArrays(GL_POINTS, 0, (GLsizei)num_points);

  glBindBuffer(GL_ARRAY_BUFFER,
               static_cast<VertexManager*>(g_vertex_manager.get())->GetVertexBufferHandle());
  g_renderer->RestoreAPIState();

  // TODO: Could just update the EFB cache with the new value
  ClearEFBCache();
}

}  // namespace OGL
