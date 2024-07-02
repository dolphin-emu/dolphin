// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/OGL/OGLGfx.h"

#include "Common/GL/GLContext.h"
#include "Common/GL/GLExtensions/GLExtensions.h"
#include "Common/Logging/LogManager.h"

#include "Core/Config/GraphicsSettings.h"

#include "VideoBackends/OGL/OGLConfig.h"
#include "VideoBackends/OGL/OGLPipeline.h"
#include "VideoBackends/OGL/OGLShader.h"
#include "VideoBackends/OGL/OGLTexture.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/SamplerCache.h"

#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoConfig.h"

#include <algorithm>
#include <string_view>

namespace OGL
{
VideoConfig g_ogl_config;

static void APIENTRY ErrorCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                   GLsizei length, const char* message, const void* userParam)
{
  const char* s_source;
  const char* s_type;

  // Performance - DualCore driver performance warning:
  // DualCore application thread syncing with server thread
  if (id == 0x200b0)
    return;

  switch (source)
  {
  case GL_DEBUG_SOURCE_API_ARB:
    s_source = "API";
    break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
    s_source = "Window System";
    break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
    s_source = "Shader Compiler";
    break;
  case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
    s_source = "Third Party";
    break;
  case GL_DEBUG_SOURCE_APPLICATION_ARB:
    s_source = "Application";
    break;
  case GL_DEBUG_SOURCE_OTHER_ARB:
    s_source = "Other";
    break;
  default:
    s_source = "Unknown";
    break;
  }
  switch (type)
  {
  case GL_DEBUG_TYPE_ERROR_ARB:
    s_type = "Error";
    break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
    s_type = "Deprecated";
    break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
    s_type = "Undefined";
    break;
  case GL_DEBUG_TYPE_PORTABILITY_ARB:
    s_type = "Portability";
    break;
  case GL_DEBUG_TYPE_PERFORMANCE_ARB:
    s_type = "Performance";
    break;
  case GL_DEBUG_TYPE_OTHER_ARB:
    s_type = "Other";
    break;
  default:
    s_type = "Unknown";
    break;
  }
  switch (severity)
  {
  case GL_DEBUG_SEVERITY_HIGH_ARB:
    ERROR_LOG_FMT(HOST_GPU, "id: {:x}, source: {}, type: {} - {}", id, s_source, s_type, message);
    break;
  case GL_DEBUG_SEVERITY_MEDIUM_ARB:
    WARN_LOG_FMT(HOST_GPU, "id: {:x}, source: {}, type: {} - {}", id, s_source, s_type, message);
    break;
  case GL_DEBUG_SEVERITY_LOW_ARB:
    DEBUG_LOG_FMT(HOST_GPU, "id: {:x}, source: {}, type: {} - {}", id, s_source, s_type, message);
    break;
  case GL_DEBUG_SEVERITY_NOTIFICATION:
    DEBUG_LOG_FMT(HOST_GPU, "id: {:x}, source: {}, type: {} - {}", id, s_source, s_type, message);
    break;
  default:
    ERROR_LOG_FMT(HOST_GPU, "id: {:x}, source: {}, type: {} - {}", id, s_source, s_type, message);
    break;
  }
}

// Two small Fallbacks to avoid GL_ARB_ES2_compatibility
static void APIENTRY DepthRangef(GLfloat neardepth, GLfloat fardepth)
{
  glDepthRange(neardepth, fardepth);
}
static void APIENTRY ClearDepthf(GLfloat depthval)
{
  glClearDepth(depthval);
}

OGLGfx::OGLGfx(std::unique_ptr<GLContext> main_gl_context, float backbuffer_scale)
    : m_main_gl_context(std::move(main_gl_context)),
      m_current_rasterization_state(RenderState::GetInvalidRasterizationState()),
      m_current_depth_state(RenderState::GetInvalidDepthState()),
      m_current_blend_state(RenderState::GetInvalidBlendingState()),
      m_backbuffer_scale(backbuffer_scale)
{
  // Create the window framebuffer.
  if (!m_main_gl_context->IsHeadless())
  {
    m_system_framebuffer = std::make_unique<OGLFramebuffer>(
        nullptr, nullptr, std::vector<AbstractTexture*>{}, AbstractTextureFormat::RGBA8,
        AbstractTextureFormat::Undefined, std::max(m_main_gl_context->GetBackBufferWidth(), 1u),
        std::max(m_main_gl_context->GetBackBufferHeight(), 1u), 1, 1, 0);
    m_current_framebuffer = m_system_framebuffer.get();
  }

  if (!m_main_gl_context->IsGLES())
  {
    // OpenGL 3 doesn't provide GLES like float functions for depth.
    // They are in core in OpenGL 4.1, so almost every driver should support them.
    // But for the oldest ones, we provide fallbacks to the old double functions.
    if (!GLExtensions::Supports("GL_ARB_ES2_compatibility"))
    {
      glDepthRangef = DepthRangef;
      glClearDepthf = ClearDepthf;
    }
  }

  InitDriverInfo();

  // Setup Debug logging
  if (g_ogl_config.bSupportsDebug)
  {
    if (GLExtensions::Supports("GL_KHR_debug"))
    {
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, true);
      glDebugMessageCallback(ErrorCallback, nullptr);
    }
    else
    {
      glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, true);
      glDebugMessageCallbackARB(ErrorCallback, nullptr);
    }
    if (Common::Log::LogManager::GetInstance()->IsEnabled(Common::Log::LogType::HOST_GPU,
                                                          Common::Log::LogLevel::LERROR))
    {
      glEnable(GL_DEBUG_OUTPUT);
    }
    else
    {
      glDisable(GL_DEBUG_OUTPUT);
    }
  }

  // Handle VSync on/off
  if (!DriverDetails::HasBug(DriverDetails::BUG_BROKEN_VSYNC))
    m_main_gl_context->SwapInterval(g_ActiveConfig.bVSyncActive);

  if (g_ActiveConfig.backend_info.bSupportsClipControl)
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

  if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
  {
    glEnable(GL_CLIP_DISTANCE0);
    glEnable(GL_CLIP_DISTANCE1);
    glEnable(GL_DEPTH_CLAMP);
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment

  glGenFramebuffers(1, &m_shared_read_framebuffer);
  glGenFramebuffers(1, &m_shared_draw_framebuffer);

  if (g_ActiveConfig.backend_info.bSupportsPrimitiveRestart)
    GLUtil::EnablePrimitiveRestart(m_main_gl_context.get());

  UpdateActiveConfig();
}

OGLGfx::~OGLGfx()
{
  glDeleteFramebuffers(1, &m_shared_draw_framebuffer);
  glDeleteFramebuffers(1, &m_shared_read_framebuffer);
}

bool OGLGfx::IsHeadless() const
{
  return m_main_gl_context->IsHeadless();
}

std::unique_ptr<AbstractTexture> OGLGfx::CreateTexture(const TextureConfig& config,
                                                       std::string_view name)
{
  return std::make_unique<OGLTexture>(config, name);
}

std::unique_ptr<AbstractStagingTexture> OGLGfx::CreateStagingTexture(StagingTextureType type,
                                                                     const TextureConfig& config)
{
  return OGLStagingTexture::Create(type, config);
}

std::unique_ptr<AbstractFramebuffer>
OGLGfx::CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                          std::vector<AbstractTexture*> additional_color_attachments)
{
  return OGLFramebuffer::Create(static_cast<OGLTexture*>(color_attachment),
                                static_cast<OGLTexture*>(depth_attachment),
                                std::move(additional_color_attachments));
}

std::unique_ptr<AbstractShader>
OGLGfx::CreateShaderFromSource(ShaderStage stage, std::string_view source, std::string_view name)
{
  return OGLShader::CreateFromSource(stage, source, name);
}

std::unique_ptr<AbstractShader>
OGLGfx::CreateShaderFromBinary(ShaderStage stage, const void* data, size_t length,
                               [[maybe_unused]] std::string_view name)
{
  return nullptr;
}

std::unique_ptr<AbstractPipeline> OGLGfx::CreatePipeline(const AbstractPipelineConfig& config,
                                                         const void* cache_data,
                                                         size_t cache_data_length)
{
  return OGLPipeline::Create(config, cache_data, cache_data_length);
}

void OGLGfx::SetScissorRect(const MathUtil::Rectangle<int>& rc)
{
  glScissor(rc.left, rc.top, rc.GetWidth(), rc.GetHeight());
}

void OGLGfx::SetViewport(float x, float y, float width, float height, float near_depth,
                         float far_depth)
{
  if (g_ogl_config.bSupportViewportFloat)
  {
    glViewportIndexedf(0, x, y, width, height);
  }
  else
  {
    auto iceilf = [](float f) { return static_cast<GLint>(std::ceil(f)); };
    glViewport(iceilf(x), iceilf(y), iceilf(width), iceilf(height));
  }

  glDepthRangef(near_depth, far_depth);
}

void OGLGfx::Draw(u32 base_vertex, u32 num_vertices)
{
  glDrawArrays(static_cast<const OGLPipeline*>(m_current_pipeline)->GetGLPrimitive(), base_vertex,
               num_vertices);
}

void OGLGfx::DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex)
{
  if (g_ogl_config.bSupportsGLBaseVertex)
  {
    glDrawElementsBaseVertex(static_cast<const OGLPipeline*>(m_current_pipeline)->GetGLPrimitive(),
                             num_indices, GL_UNSIGNED_SHORT,
                             static_cast<u16*>(nullptr) + base_index, base_vertex);
  }
  else
  {
    glDrawElements(static_cast<const OGLPipeline*>(m_current_pipeline)->GetGLPrimitive(),
                   num_indices, GL_UNSIGNED_SHORT, static_cast<u16*>(nullptr) + base_index);
  }
}

void OGLGfx::DispatchComputeShader(const AbstractShader* shader, u32 groupsize_x, u32 groupsize_y,
                                   u32 groupsize_z, u32 groups_x, u32 groups_y, u32 groups_z)
{
  glUseProgram(static_cast<const OGLShader*>(shader)->GetGLComputeProgramID());
  glDispatchCompute(groups_x, groups_y, groups_z);

  // We messed up the program binding, so restore it.
  ProgramShaderCache::InvalidateLastProgram();
  if (m_current_pipeline)
    static_cast<const OGLPipeline*>(m_current_pipeline)->GetProgram()->shader.Bind();

  // Barrier to texture can be used for reads.
  if (std::any_of(m_bound_image_textures.begin(), m_bound_image_textures.end(),
                  [](auto image) { return image != nullptr; }))
  {
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
  }
}

void OGLGfx::SelectLeftBuffer()
{
  glDrawBuffer(GL_BACK_LEFT);
}

void OGLGfx::SelectRightBuffer()
{
  glDrawBuffer(GL_BACK_RIGHT);
}

void OGLGfx::SelectMainBuffer()
{
  glDrawBuffer(GL_BACK);
}

void OGLGfx::SetFramebuffer(AbstractFramebuffer* framebuffer)
{
  if (m_current_framebuffer == framebuffer)
    return;

  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<OGLFramebuffer*>(framebuffer)->GetFBO());
  m_current_framebuffer = framebuffer;
}

void OGLGfx::SetAndDiscardFramebuffer(AbstractFramebuffer* framebuffer)
{
  // EXT_discard_framebuffer could be used here to save bandwidth on tilers.
  SetFramebuffer(framebuffer);
}

void OGLGfx::SetAndClearFramebuffer(AbstractFramebuffer* framebuffer, const ClearColor& color_value,
                                    float depth_value)
{
  SetFramebuffer(framebuffer);

  glDisable(GL_SCISSOR_TEST);
  GLbitfield clear_mask = 0;
  if (framebuffer->HasColorBuffer())
  {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(color_value[0], color_value[1], color_value[2], color_value[3]);
    clear_mask |= GL_COLOR_BUFFER_BIT;
  }
  if (framebuffer->HasDepthBuffer())
  {
    glDepthMask(GL_TRUE);
    glClearDepthf(depth_value);
    clear_mask |= GL_DEPTH_BUFFER_BIT;
  }
  glClear(clear_mask);
  glEnable(GL_SCISSOR_TEST);

  // Restore color/depth mask.
  if (framebuffer->HasColorBuffer())
  {
    glColorMask(m_current_blend_state.colorupdate, m_current_blend_state.colorupdate,
                m_current_blend_state.colorupdate, m_current_blend_state.alphaupdate);
  }
  if (framebuffer->HasDepthBuffer())
    glDepthMask(m_current_depth_state.updateenable);
}

void OGLGfx::ClearRegion(const MathUtil::Rectangle<int>& target_rc, bool colorEnable,
                         bool alphaEnable, bool zEnable, u32 color, u32 z)
{
  u32 clear_mask = 0;
  if (colorEnable || alphaEnable)
  {
    glColorMask(colorEnable, colorEnable, colorEnable, alphaEnable);
    glClearColor(float((color >> 16) & 0xFF) / 255.0f, float((color >> 8) & 0xFF) / 255.0f,
                 float((color >> 0) & 0xFF) / 255.0f, float((color >> 24) & 0xFF) / 255.0f);
    clear_mask = GL_COLOR_BUFFER_BIT;
  }
  if (zEnable)
  {
    glDepthMask(zEnable ? GL_TRUE : GL_FALSE);
    glClearDepthf(float(z & 0xFFFFFF) / 16777216.0f);
    clear_mask |= GL_DEPTH_BUFFER_BIT;
  }

  // Update rect for clearing the picture
  // glColorMask/glDepthMask/glScissor affect glClear (glViewport does not)
  g_gfx->SetScissorRect(target_rc);

  glClear(clear_mask);

  // Restore color/depth mask.
  if (colorEnable || alphaEnable)
  {
    glColorMask(m_current_blend_state.colorupdate, m_current_blend_state.colorupdate,
                m_current_blend_state.colorupdate, m_current_blend_state.alphaupdate);
  }
  if (zEnable)
    glDepthMask(m_current_depth_state.updateenable);
}

void OGLGfx::BindBackbuffer(const ClearColor& clear_color)
{
  CheckForSurfaceChange();
  CheckForSurfaceResize();
  SetAndClearFramebuffer(m_system_framebuffer.get(), clear_color);
}

void OGLGfx::PresentBackbuffer()
{
  if (g_ogl_config.bSupportsDebug)
  {
    if (Common::Log::LogManager::GetInstance()->IsEnabled(Common::Log::LogType::HOST_GPU,
                                                          Common::Log::LogLevel::LERROR))
    {
      glEnable(GL_DEBUG_OUTPUT);
    }
    else
    {
      glDisable(GL_DEBUG_OUTPUT);
    }
  }

  // Swap the back and front buffers, presenting the image.
  m_main_gl_context->Swap();
}

void OGLGfx::OnConfigChanged(u32 bits)
{
  AbstractGfx::OnConfigChanged(bits);

  if (bits & CONFIG_CHANGE_BIT_VSYNC && !DriverDetails::HasBug(DriverDetails::BUG_BROKEN_VSYNC))
    m_main_gl_context->SwapInterval(g_ActiveConfig.bVSyncActive);

  if (bits & CONFIG_CHANGE_BIT_ANISOTROPY)
    g_sampler_cache->Clear();
}

void OGLGfx::Flush(FlushType flushType)
{
  if (flushType == FlushType::FlushToWorker)
    return;

  // ensure all commands are sent to the GPU.
  // Otherwise the driver could batch several frames together.
  glFlush();
}

void OGLGfx::WaitForGPUIdle()
{
  glFinish();
}

void OGLGfx::CheckForSurfaceChange()
{
  if (!g_presenter->SurfaceChangedTestAndClear())
    return;

  m_main_gl_context->UpdateSurface(g_presenter->GetNewSurfaceHandle());

  u32 width = m_main_gl_context->GetBackBufferWidth();
  u32 height = m_main_gl_context->GetBackBufferHeight();

  // With a surface change, the window likely has new dimensions.
  g_presenter->SetBackbuffer(width, height);
  m_system_framebuffer->UpdateDimensions(width, height);
}

void OGLGfx::CheckForSurfaceResize()
{
  if (!g_presenter->SurfaceResizedTestAndClear())
    return;

  m_main_gl_context->Update();
  u32 width = m_main_gl_context->GetBackBufferWidth();
  u32 height = m_main_gl_context->GetBackBufferHeight();
  g_presenter->SetBackbuffer(width, height);
  m_system_framebuffer->UpdateDimensions(width, height);
}

void OGLGfx::BeginUtilityDrawing()
{
  AbstractGfx::BeginUtilityDrawing();
  if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
  {
    glDisable(GL_CLIP_DISTANCE0);
    glDisable(GL_CLIP_DISTANCE1);
  }
}

void OGLGfx::EndUtilityDrawing()
{
  AbstractGfx::EndUtilityDrawing();
  if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
  {
    glEnable(GL_CLIP_DISTANCE0);
    glEnable(GL_CLIP_DISTANCE1);
  }
}

void OGLGfx::ApplyRasterizationState(const RasterizationState state)
{
  if (m_current_rasterization_state == state)
    return;

  // none, ccw, cw, ccw
  if (state.cullmode != CullMode::None)
  {
    // TODO: GX_CULL_ALL not supported, yet!
    glEnable(GL_CULL_FACE);
    glFrontFace(state.cullmode == CullMode::Front ? GL_CCW : GL_CW);
  }
  else
  {
    glDisable(GL_CULL_FACE);
  }

  m_current_rasterization_state = state;
}

void OGLGfx::ApplyDepthState(const DepthState state)
{
  if (m_current_depth_state == state)
    return;

  const GLenum glCmpFuncs[8] = {GL_NEVER,   GL_LESS,     GL_EQUAL,  GL_LEQUAL,
                                GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS};

  if (state.testenable)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(state.updateenable ? GL_TRUE : GL_FALSE);
    glDepthFunc(glCmpFuncs[u32(state.func.Value())]);
  }
  else
  {
    // if the test is disabled write is disabled too
    // TODO: When PE performance metrics are being emulated via occlusion queries, we should
    // (probably?) enable depth test with depth function ALWAYS here
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
  }

  m_current_depth_state = state;
}

void OGLGfx::ApplyBlendingState(const BlendingState state)
{
  if (m_current_blend_state == state)
    return;

  bool useDualSource = state.usedualsrc;

  const GLenum src_factors[8] = {GL_ZERO,
                                 GL_ONE,
                                 GL_DST_COLOR,
                                 GL_ONE_MINUS_DST_COLOR,
                                 useDualSource ? GL_SRC1_ALPHA : (GLenum)GL_SRC_ALPHA,
                                 useDualSource ? GL_ONE_MINUS_SRC1_ALPHA :
                                                 (GLenum)GL_ONE_MINUS_SRC_ALPHA,
                                 GL_DST_ALPHA,
                                 GL_ONE_MINUS_DST_ALPHA};
  const GLenum dst_factors[8] = {GL_ZERO,
                                 GL_ONE,
                                 GL_SRC_COLOR,
                                 GL_ONE_MINUS_SRC_COLOR,
                                 useDualSource ? GL_SRC1_ALPHA : (GLenum)GL_SRC_ALPHA,
                                 useDualSource ? GL_ONE_MINUS_SRC1_ALPHA :
                                                 (GLenum)GL_ONE_MINUS_SRC_ALPHA,
                                 GL_DST_ALPHA,
                                 GL_ONE_MINUS_DST_ALPHA};

  if (state.blendenable)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);

  // Always call glBlendEquationSeparate and glBlendFuncSeparate, even when
  // GL_BLEND is disabled, as a workaround for some bugs (possibly graphics
  // driver issues?). See https://bugs.dolphin-emu.org/issues/10120 : "Sonic
  // Adventure 2 Battle: graphics crash when loading first Dark level"
  GLenum equation = state.subtract ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD;
  GLenum equationAlpha = state.subtractAlpha ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD;
  glBlendEquationSeparate(equation, equationAlpha);
  glBlendFuncSeparate(src_factors[u32(state.srcfactor.Value())],
                      dst_factors[u32(state.dstfactor.Value())],
                      src_factors[u32(state.srcfactoralpha.Value())],
                      dst_factors[u32(state.dstfactoralpha.Value())]);

  const GLenum logic_op_codes[16] = {
      GL_CLEAR,         GL_AND,         GL_AND_REVERSE, GL_COPY,  GL_AND_INVERTED, GL_NOOP,
      GL_XOR,           GL_OR,          GL_NOR,         GL_EQUIV, GL_INVERT,       GL_OR_REVERSE,
      GL_COPY_INVERTED, GL_OR_INVERTED, GL_NAND,        GL_SET};

  // Logic ops aren't available in GLES3
  if (!IsGLES())
  {
    if (state.logicopenable)
    {
      glEnable(GL_COLOR_LOGIC_OP);
      glLogicOp(logic_op_codes[u32(state.logicmode.Value())]);
    }
    else
    {
      glDisable(GL_COLOR_LOGIC_OP);
    }
  }

  glColorMask(state.colorupdate, state.colorupdate, state.colorupdate, state.alphaupdate);
  m_current_blend_state = state;
}

void OGLGfx::SetPipeline(const AbstractPipeline* pipeline)
{
  if (m_current_pipeline == pipeline)
    return;

  if (pipeline)
  {
    ApplyRasterizationState(static_cast<const OGLPipeline*>(pipeline)->GetRasterizationState());
    ApplyDepthState(static_cast<const OGLPipeline*>(pipeline)->GetDepthState());
    ApplyBlendingState(static_cast<const OGLPipeline*>(pipeline)->GetBlendingState());
    ProgramShaderCache::BindVertexFormat(
        static_cast<const OGLPipeline*>(pipeline)->GetVertexFormat());
    static_cast<const OGLPipeline*>(pipeline)->GetProgram()->shader.Bind();
  }
  else
  {
    ProgramShaderCache::InvalidateLastProgram();
    glUseProgram(0);
  }
  m_current_pipeline = pipeline;
}

void OGLGfx::SetTexture(u32 index, const AbstractTexture* texture)
{
  const OGLTexture* gl_texture = static_cast<const OGLTexture*>(texture);
  if (m_bound_textures[index] == gl_texture)
    return;

  glActiveTexture(GL_TEXTURE0 + index);
  if (gl_texture)
    glBindTexture(gl_texture->GetGLTarget(), gl_texture->GetGLTextureId());
  else
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  m_bound_textures[index] = gl_texture;
}

void OGLGfx::SetSamplerState(u32 index, const SamplerState& state)
{
  g_sampler_cache->SetSamplerState(index, state);
}

void OGLGfx::SetComputeImageTexture(u32 index, AbstractTexture* texture, bool read, bool write)
{
  if (m_bound_image_textures[index] == texture)
    return;

  if (texture)
  {
    const GLenum access = read ? (write ? GL_READ_WRITE : GL_READ_ONLY) : GL_WRITE_ONLY;
    glBindImageTexture(index, static_cast<OGLTexture*>(texture)->GetGLTextureId(), 0, GL_TRUE, 0,
                       access, static_cast<OGLTexture*>(texture)->GetGLFormatForImageTexture());
  }
  else
  {
    glBindImageTexture(index, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
  }

  m_bound_image_textures[index] = texture;
}

void OGLGfx::UnbindTexture(const AbstractTexture* texture)
{
  for (size_t i = 0; i < m_bound_textures.size(); i++)
  {
    if (m_bound_textures[i] != texture)
      continue;

    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + i));
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    m_bound_textures[i] = nullptr;
  }

  for (size_t i = 0; i < m_bound_image_textures.size(); i++)
  {
    if (m_bound_image_textures[i] != texture)
      continue;

    glBindImageTexture(static_cast<GLuint>(i), 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    m_bound_image_textures[i] = nullptr;
  }
}

std::unique_ptr<VideoCommon::AsyncShaderCompiler> OGLGfx::CreateAsyncShaderCompiler()
{
  return std::make_unique<SharedContextAsyncShaderCompiler>();
}

bool OGLGfx::IsGLES() const
{
  return m_main_gl_context->IsGLES();
}

void OGLGfx::BindSharedReadFramebuffer()
{
  glBindFramebuffer(GL_READ_FRAMEBUFFER, m_shared_read_framebuffer);
}

void OGLGfx::BindSharedDrawFramebuffer()
{
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_shared_draw_framebuffer);
}

void OGLGfx::RestoreFramebufferBinding()
{
  glBindFramebuffer(
      GL_FRAMEBUFFER,
      m_current_framebuffer ? static_cast<OGLFramebuffer*>(m_current_framebuffer)->GetFBO() : 0);
}

SurfaceInfo OGLGfx::GetSurfaceInfo() const
{
  return {std::max(m_main_gl_context->GetBackBufferWidth(), 1u),
          std::max(m_main_gl_context->GetBackBufferHeight(), 1u), m_backbuffer_scale,
          AbstractTextureFormat::RGBA8};
}

}  // namespace OGL
