// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>

#include "Common/GL/GLContext.h"
#include "Common/GL/GLExtensions/GLExtensions.h"
#include "VideoCommon/RenderBase.h"

namespace OGL
{
class OGLFramebuffer;
class OGLPipeline;
class OGLTexture;

enum GlslVersion
{
  Glsl130,
  Glsl140,
  Glsl150,
  Glsl330,
  Glsl400,  // and above
  Glsl430,
  GlslEs300,  // GLES 3.0
  GlslEs310,  // GLES 3.1
  GlslEs320,  // GLES 3.2
};
enum class EsTexbufType
{
  TexbufNone,
  TexbufCore,
  TexbufOes,
  TexbufExt
};

enum class EsFbFetchType
{
  FbFetchNone,
  FbFetchExt,
  FbFetchArm,
};

// ogl-only config, so not in VideoConfig.h
struct VideoConfig
{
  bool bIsES;
  bool bSupportsGLPinnedMemory;
  bool bSupportsGLSync;
  bool bSupportsGLBaseVertex;
  bool bSupportsGLBufferStorage;
  bool bSupportsMSAA;
  GlslVersion eSupportedGLSLVersion;
  bool bSupportViewportFloat;
  bool bSupportsAEP;
  bool bSupportsDebug;
  bool bSupportsCopySubImage;
  u8 SupportedESPointSize;
  EsTexbufType SupportedESTextureBuffer;
  bool bSupportsTextureStorage;
  bool bSupports2DTextureStorageMultisample;
  bool bSupports3DTextureStorageMultisample;
  bool bSupportsConservativeDepth;
  bool bSupportsImageLoadStore;
  bool bSupportsAniso;
  bool bSupportsBitfield;
  bool bSupportsTextureSubImage;
  EsFbFetchType SupportedFramebufferFetch;
  bool bSupportsShaderThreadShuffleNV;

  const char* gl_vendor;
  const char* gl_renderer;
  const char* gl_version;

  s32 max_samples;
};
extern VideoConfig g_ogl_config;

class Renderer : public ::Renderer
{
public:
  Renderer(std::unique_ptr<GLContext> main_gl_context, float backbuffer_scale);
  ~Renderer() override;

  static Renderer* GetInstance() { return static_cast<Renderer*>(g_renderer.get()); }

  bool IsHeadless() const override;

  void Shutdown() override;

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;
  std::unique_ptr<AbstractShader> CreateShaderFromSource(ShaderStage stage,
                                                         std::string_view source) override;
  std::unique_ptr<AbstractShader> CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                         size_t length) override;
  std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;
  std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config,
                                                   const void* cache_data = nullptr,
                                                   size_t cache_data_length = 0) override;
  std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment) override;

  void SetPipeline(const AbstractPipeline* pipeline) override;
  void SetFramebuffer(AbstractFramebuffer* framebuffer) override;
  void SetAndDiscardFramebuffer(AbstractFramebuffer* framebuffer) override;
  void SetAndClearFramebuffer(AbstractFramebuffer* framebuffer, const ClearColor& color_value = {},
                              float depth_value = 0.0f) override;
  void SetScissorRect(const MathUtil::Rectangle<int>& rc) override;
  void SetTexture(u32 index, const AbstractTexture* texture) override;
  void SetSamplerState(u32 index, const SamplerState& state) override;
  void SetComputeImageTexture(AbstractTexture* texture, bool read, bool write) override;
  void UnbindTexture(const AbstractTexture* texture) override;
  void SetViewport(float x, float y, float width, float height, float near_depth,
                   float far_depth) override;
  void Draw(u32 base_vertex, u32 num_vertices) override;
  void DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex) override;
  void DispatchComputeShader(const AbstractShader* shader, u32 groups_x, u32 groups_y,
                             u32 groups_z) override;
  void BindBackbuffer(const ClearColor& clear_color = {}) override;
  void PresentBackbuffer() override;

  u16 BBoxRead(int index) override;
  void BBoxWrite(int index, u16 value) override;

  void BeginUtilityDrawing() override;
  void EndUtilityDrawing() override;

  void Flush() override;
  void WaitForGPUIdle() override;
  void OnConfigChanged(u32 bits) override;

  void ClearScreen(const MathUtil::Rectangle<int>& rc, bool colorEnable, bool alphaEnable,
                   bool zEnable, u32 color, u32 z) override;

  std::unique_ptr<VideoCommon::AsyncShaderCompiler> CreateAsyncShaderCompiler() override;

  // Only call methods from this on the GPU thread.
  GLContext* GetMainGLContext() const { return m_main_gl_context.get(); }
  bool IsGLES() const { return m_main_gl_context->IsGLES(); }

  // Invalidates a cached texture binding. Required for texel buffers when they borrow the units.
  void InvalidateTextureBinding(u32 index) { m_bound_textures[index] = nullptr; }

  // The shared framebuffer exists for copying textures when extensions are not available. It is
  // slower, but the only way to do these things otherwise.
  GLuint GetSharedReadFramebuffer() const { return m_shared_read_framebuffer; }
  GLuint GetSharedDrawFramebuffer() const { return m_shared_draw_framebuffer; }
  void BindSharedReadFramebuffer();
  void BindSharedDrawFramebuffer();

  // Restores FBO binding after it's been changed.
  void RestoreFramebufferBinding();

private:
  void CheckForSurfaceChange();
  void CheckForSurfaceResize();

  void ApplyRasterizationState(const RasterizationState state);
  void ApplyDepthState(const DepthState state);
  void ApplyBlendingState(const BlendingState state);

  std::unique_ptr<GLContext> m_main_gl_context;
  std::unique_ptr<OGLFramebuffer> m_system_framebuffer;
  std::array<const OGLTexture*, 8> m_bound_textures{};
  AbstractTexture* m_bound_image_texture = nullptr;
  RasterizationState m_current_rasterization_state;
  DepthState m_current_depth_state;
  BlendingState m_current_blend_state;
  GLuint m_shared_read_framebuffer = 0;
  GLuint m_shared_draw_framebuffer = 0;
};
}  // namespace OGL
