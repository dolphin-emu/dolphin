// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoCommon/RenderBase.h"

namespace Vulkan
{
class BoundingBox;
class FramebufferManager;
class SwapChain;
class StagingTexture2D;
class StateTracker;
class Texture2D;
class RasterFont;

class Renderer : public ::Renderer
{
public:
  Renderer(std::unique_ptr<SwapChain> swap_chain);
  ~Renderer();

  SwapChain* GetSwapChain() const { return m_swap_chain.get(); }
  StateTracker* GetStateTracker() const { return m_state_tracker.get(); }
  BoundingBox* GetBoundingBox() const { return m_bounding_box.get(); }
  bool Initialize(FramebufferManager* framebuffer_mgr);

  void RenderText(const std::string& pstr, int left, int top, u32 color) override;
  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override;
  u16 BBoxRead(int index) override;
  void BBoxWrite(int index, u16 value) override;
  u32 GetMaxTextureSize() override { return 16 * 1024; }
  TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

  void SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, const EFBRectangle& rc,
                float gamma) override;

  void ClearScreen(const EFBRectangle& rc, bool color_enable, bool alpha_enable, bool z_enable,
                   u32 color, u32 z) override;

  void ReinterpretPixelData(unsigned int convtype) override;

  void ApplyState(bool bUseDstAlpha) override;

  void ResetAPIState() override;
  void RestoreAPIState() override;

  void SetColorMask() override;
  void SetBlendMode(bool force_update) override;
  void SetScissorRect(const EFBRectangle& rc) override;
  void SetGenerationMode() override;
  void SetDepthMode() override;
  void SetLogicOpMode() override;
  void SetDitherMode() override;
  void SetSamplerState(int stage, int texindex, bool custom_tex) override;
  void SetInterlacingMode() override;
  void SetViewport() override;

  void ChangeSurface(void* new_surface_handle) override;

private:
  bool CreateSemaphores();
  void DestroySemaphores();

  void BeginFrame();

  void CheckForTargetResize(u32 fb_width, u32 fb_stride, u32 fb_height);
  void CheckForSurfaceChange();
  void CheckForConfigChanges();

  void ResetSamplerStates();

  void OnSwapChainResized();
  void BindEFBToStateTracker();
  void ResizeEFBTextures();
  void ResizeSwapChain();

  void RecompileShaders();
  bool CompileShaders();
  void DestroyShaders();

  void DrawScreen(const TargetRectangle& src_rect, const Texture2D* src_tex);
  bool DrawScreenshot(const TargetRectangle& src_rect, const Texture2D* src_tex);
  void BlitScreen(VkRenderPass render_pass, const TargetRectangle& dst_rect,
                  const TargetRectangle& src_rect, const Texture2D* src_tex, bool linear_filter);
  bool ResizeScreenshotBuffer(u32 new_width, u32 new_height);
  void DestroyScreenshotResources();
  FramebufferManager* m_framebuffer_mgr = nullptr;

  VkSemaphore m_image_available_semaphore = VK_NULL_HANDLE;
  VkSemaphore m_rendering_finished_semaphore = VK_NULL_HANDLE;

  std::unique_ptr<SwapChain> m_swap_chain;
  std::unique_ptr<StateTracker> m_state_tracker;
  std::unique_ptr<BoundingBox> m_bounding_box;
  std::unique_ptr<RasterFont> m_raster_font;

  // Keep a copy of sampler states to avoid cache lookups every draw
  std::array<SamplerState, NUM_PIXEL_SHADER_SAMPLERS> m_sampler_states = {};

  // Shaders used for clear/blit.
  VkShaderModule m_clear_fragment_shader = VK_NULL_HANDLE;

  // NOTE: The blit shader here is used for the final copy from the source buffer(s) to the swap
  // chain buffer for presentation. It ignores the alpha channel of the input image and sets the
  // alpha channel to 1.0 to avoid issues with frame dumping and screenshots.
  VkShaderModule m_blit_fragment_shader = VK_NULL_HANDLE;

  // Texture used for screenshot/frame dumping
  std::unique_ptr<Texture2D> m_screenshot_render_texture;
  std::unique_ptr<StagingTexture2D> m_screenshot_readback_texture;
  VkFramebuffer m_screenshot_framebuffer = VK_NULL_HANDLE;
};
}
