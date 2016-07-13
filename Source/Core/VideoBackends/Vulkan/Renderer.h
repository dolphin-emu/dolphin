// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/VulkanImports.h"
#include "VideoCommon/RenderBase.h"

namespace Vulkan
{
class BoundingBox;
class EFBCache;
class FramebufferManager;
class SwapChain;
class StateTracker;
class RasterFont;

class Renderer : public ::Renderer
{
public:
  Renderer(SwapChain* swap_chain, StateTracker* state_tracker);
  ~Renderer();

  bool Initialize();

  void RenderText(const std::string& pstr, int left, int top, u32 color) override;
  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override;
  u16 BBoxRead(int index) override;
  void BBoxWrite(int index, u16 value) override;
  int GetMaxTextureSize() override { return 16 * 1024; }
  TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

  void SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, const EFBRectangle& rc,
                float gamma) override;

  void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                   u32 color, u32 z) override;

  void ReinterpretPixelData(unsigned int convtype) override;

  bool SaveScreenshot(const std::string& filename, const TargetRectangle& rc) override
  {
    return false;
  }

  void ApplyState(bool bUseDstAlpha) override;

  void ResetAPIState() override;
  void RestoreAPIState() override;

  void SetColorMask() override;
  void SetBlendMode(bool forceUpdate) override;
  void SetScissorRect(const EFBRectangle& rc) override;
  void SetGenerationMode() override;
  void SetDepthMode() override;
  void SetLogicOpMode() override;
  void SetDitherMode() override;
  void SetSamplerState(int stage, int texindex, bool custom_tex) override;
  void SetInterlacingMode() override;
  void SetViewport() override;

  void ChangeSurface(void* new_window_handle) override;

  EFBCache* GetEFBCache() const { return m_efb_cache.get(); }
  BoundingBox* GetBoundingBox() const { return m_bounding_box.get(); }
private:
  bool CreateSemaphores();
  void DestroySemaphores();

  void BeginFrame();

  void CheckForConfigChanges();
  void CheckForSurfaceChange();

  void ResetSamplerStates();

  void OnSwapChainResized();
  void ResizeEFBTextures();
  void ResizeSwapChain();

  SwapChain* m_swap_chain = nullptr;
  StateTracker* m_state_tracker = nullptr;

  FramebufferManager* m_framebuffer_mgr = nullptr;

  VkSemaphore m_image_available_semaphore = nullptr;
  VkSemaphore m_rendering_finished_semaphore = nullptr;

  std::unique_ptr<RasterFont> m_raster_font;
  std::unique_ptr<EFBCache> m_efb_cache;
  std::unique_ptr<BoundingBox> m_bounding_box;

  // Keep a copy of sampler states to avoid cache lookups every draw
  std::array<SamplerState, NUM_PIXEL_SHADER_SAMPLERS> m_sampler_states = {};

  // Surface changes, needed for android. Called off-thread.
  void* m_new_window_handle = nullptr;
};
}
