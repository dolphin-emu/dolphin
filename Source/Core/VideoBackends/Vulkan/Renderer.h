// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <tuple>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoCommon/RenderBase.h"

struct XFBSourceBase;

namespace Vulkan
{
class BoundingBox;
class FramebufferManager;
class SwapChain;
class StagingTexture2D;
class Texture2D;
class RasterFont;
class VKFramebuffer;
class VKPipeline;
class VKTexture;

class Renderer : public ::Renderer
{
public:
  Renderer(std::unique_ptr<SwapChain> swap_chain);
  ~Renderer() override;

  static Renderer* GetInstance();

  bool IsHeadless() const override;

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;
  std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(const AbstractTexture* color_attachment,
                    const AbstractTexture* depth_attachment) override;

  std::unique_ptr<AbstractShader> CreateShaderFromSource(ShaderStage stage, const char* source,
                                                         size_t length) override;
  std::unique_ptr<AbstractShader> CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                         size_t length) override;
  std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config) override;

  SwapChain* GetSwapChain() const { return m_swap_chain.get(); }
  BoundingBox* GetBoundingBox() const { return m_bounding_box.get(); }
  bool Initialize();

  void RenderText(const std::string& pstr, int left, int top, u32 color) override;
  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override;
  u16 BBoxRead(int index) override;
  void BBoxWrite(int index, u16 value) override;
  TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

  void SwapImpl(AbstractTexture* texture, const EFBRectangle& rc, u64 ticks) override;

  void ClearScreen(const EFBRectangle& rc, bool color_enable, bool alpha_enable, bool z_enable,
                   u32 color, u32 z) override;

  void ReinterpretPixelData(unsigned int convtype) override;

  void ApplyState() override;

  void ResetAPIState() override;
  void RestoreAPIState() override;

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

  void DrawUtilityPipeline(const void* uniforms, u32 uniforms_size, const void* vertices,
                           u32 vertex_stride, u32 num_vertices) override;
  void DispatchComputeShader(const AbstractShader* shader, const void* uniforms, u32 uniforms_size,
                             u32 groups_x, u32 groups_y, u32 groups_z) override;

private:
  bool CreateSemaphores();
  void DestroySemaphores();

  void BeginFrame();

  void CheckForSurfaceChange();
  void CheckForSurfaceResize();
  void CheckForConfigChanges();

  void ResetSamplerStates();

  void OnSwapChainResized();
  void BindEFBToStateTracker();
  void RecreateEFBFramebuffer();
  void BindFramebuffer(const VKFramebuffer* fb);

  void RecompileShaders();
  bool CompileShaders();
  void DestroyShaders();

  // Draw the frame, as well as the OSD to the swap chain.
  void DrawScreen(VKTexture* xfb_texture, const EFBRectangle& xfb_region);

  // Copies/scales an image to the currently-bound framebuffer.
  void BlitScreen(VkRenderPass render_pass, const TargetRectangle& dst_rect,
                  const TargetRectangle& src_rect, const Texture2D* src_tex);

  std::tuple<VkBuffer, u32> UpdateUtilityUniformBuffer(const void* uniforms, u32 uniforms_size);

  VkSemaphore m_image_available_semaphore = VK_NULL_HANDLE;
  VkSemaphore m_rendering_finished_semaphore = VK_NULL_HANDLE;

  std::unique_ptr<SwapChain> m_swap_chain;
  std::unique_ptr<BoundingBox> m_bounding_box;
  std::unique_ptr<RasterFont> m_raster_font;

  // Keep a copy of sampler states to avoid cache lookups every draw
  std::array<SamplerState, NUM_PIXEL_SHADER_SAMPLERS> m_sampler_states = {};

  // Shaders used for clear/blit.
  VkShaderModule m_clear_fragment_shader = VK_NULL_HANDLE;
};
}
