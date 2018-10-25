// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>

#include "Common/GL/GLContext.h"
#include "Common/GL/GLExtensions/GLExtensions.h"
#include "VideoCommon/RenderBase.h"

struct XFBSourceBase;

namespace OGL
{
class OGLPipeline;
void ClearEFBCache();

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
  bool bSupportsGLSLCache;
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

  const char* gl_vendor;
  const char* gl_renderer;
  const char* gl_version;

  s32 max_samples;
};
extern VideoConfig g_ogl_config;

class Renderer : public ::Renderer
{
public:
  Renderer(std::unique_ptr<GLContext> main_gl_context);
  ~Renderer() override;

  bool IsHeadless() const override;

  void Init();
  void Shutdown() override;

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;
  std::unique_ptr<AbstractShader> CreateShaderFromSource(ShaderStage stage, const char* source,
                                                         size_t length) override;
  std::unique_ptr<AbstractShader> CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                         size_t length) override;
  std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config) override;
  std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(const AbstractTexture* color_attachment,
                    const AbstractTexture* depth_attachment) override;

  void SetPipeline(const AbstractPipeline* pipeline) override;
  void SetFramebuffer(const AbstractFramebuffer* framebuffer) override;
  void SetAndDiscardFramebuffer(const AbstractFramebuffer* framebuffer) override;
  void SetAndClearFramebuffer(const AbstractFramebuffer* framebuffer,
                              const ClearColor& color_value = {},
                              float depth_value = 0.0f) override;
  void SetScissorRect(const MathUtil::Rectangle<int>& rc) override;
  void SetTexture(u32 index, const AbstractTexture* texture) override;
  void SetSamplerState(u32 index, const SamplerState& state) override;
  void UnbindTexture(const AbstractTexture* texture) override;
  void SetInterlacingMode() override;
  void SetViewport(float x, float y, float width, float height, float near_depth,
                   float far_depth) override;

  void RenderText(const std::string& text, int left, int top, u32 color) override;

  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override;

  u16 BBoxRead(int index) override;
  void BBoxWrite(int index, u16 value) override;

  void ResetAPIState() override;
  void RestoreAPIState() override;

  TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

  void SwapImpl(AbstractTexture* texture, const EFBRectangle& rc, u64 ticks) override;

  void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                   u32 color, u32 z) override;

  void ReinterpretPixelData(unsigned int convtype) override;

  void DrawUtilityPipeline(const void* uniforms, u32 uniforms_size, const void* vertices,
                           u32 vertex_stride, u32 num_vertices) override;

  void DispatchComputeShader(const AbstractShader* shader, const void* uniforms, u32 uniforms_size,
                             u32 groups_x, u32 groups_y, u32 groups_z) override;

  std::unique_ptr<VideoCommon::AsyncShaderCompiler> CreateAsyncShaderCompiler() override;

  // Only call methods from this on the GPU thread.
  GLContext* GetMainGLContext() const { return m_main_gl_context.get(); }
  bool IsGLES() const { return m_main_gl_context->IsGLES(); }

private:
  void UpdateEFBCache(EFBAccessType type, u32 cacheRectIdx, const EFBRectangle& efbPixelRc,
                      const TargetRectangle& targetPixelRc, const void* data);

  void DrawEFB(GLuint framebuffer, const TargetRectangle& target_rc,
               const TargetRectangle& source_rc);

  void BlitScreen(TargetRectangle src, TargetRectangle dst, GLuint src_texture, int src_width,
                  int src_height);

  void CheckForSurfaceChange();
  void CheckForSurfaceResize();

  void ApplyBlendingState(const BlendingState state, bool force = false);
  void ApplyRasterizationState(const RasterizationState state, bool force = false);
  void ApplyDepthState(const DepthState state, bool force = false);
  void UploadUtilityUniforms(const void* uniforms, u32 uniforms_size);

  std::unique_ptr<GLContext> m_main_gl_context;
  std::array<const AbstractTexture*, 8> m_bound_textures{};
  const OGLPipeline* m_graphics_pipeline = nullptr;
  RasterizationState m_current_rasterization_state = {};
  DepthState m_current_depth_state = {};
  BlendingState m_current_blend_state = {};
};
}  // namespace OGL
