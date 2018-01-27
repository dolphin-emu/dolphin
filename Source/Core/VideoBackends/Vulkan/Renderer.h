// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoCommon/AVIDump.h"
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

class Renderer : public ::Renderer
{
public:
  Renderer(std::unique_ptr<SwapChain> swap_chain);
  ~Renderer() override;

  static Renderer* GetInstance();

  SwapChain* GetSwapChain() const { return m_swap_chain.get(); }
  BoundingBox* GetBoundingBox() const { return m_bounding_box.get(); }
  bool Initialize();

  void RenderText(const std::string& pstr, int left, int top, u32 color) override;
  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override;
  u16 BBoxRead(int index) override;
  void BBoxWrite(int index, u16 value) override;
  TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

  void AsyncTimewarpDraw() override;
  void SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, const EFBRectangle& rc,
                u64 ticks, float gamma) override;

  void ClearScreen(const EFBRectangle& rc, bool color_enable, bool alpha_enable, bool z_enable,
                   u32 color, u32 z) override;
  void SkipClearScreen(bool colorEnable, bool alphaEnable, bool zEnable) override;

  void ReinterpretPixelData(unsigned int convtype) override;

  void ApplyState() override;

  void ResetAPIState() override;
  void RestoreAPIState() override;

  void SetBlendingState(const BlendingState& state) override;
  void SetScissorRect(const EFBRectangle& rc) override;
  void SetRasterizationState(const RasterizationState& state) override;
  void SetDepthState(const DepthState& state) override;
  void SetSamplerState(u32 index, const SamplerState& state) override;
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

  void RecompileShaders();
  bool CompileShaders();
  void DestroyShaders();

  // Transitions EFB/XFB buffers to SHADER_READ_ONLY, ready for presenting/dumping.
  // If MSAA is enabled, and XFB is disabled, also resolves the EFB buffer.
  void TransitionBuffersForSwap(const TargetRectangle& scaled_rect,
                                const XFBSourceBase* const* xfb_sources, u32 xfb_count);

  // Draw either the EFB, or specified XFB sources to the currently-bound framebuffer.
  void DrawFrame(VkRenderPass render_pass, const TargetRectangle& target_rect,
                 const TargetRectangle& scaled_efb_rect, u32 xfb_addr,
                 const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
                 u32 fb_stride, u32 fb_height);
  void DrawEFB(VkRenderPass render_pass, const TargetRectangle& target_rect,
               const TargetRectangle& scaled_efb_rect);
  void DrawVirtualXFB(VkRenderPass render_pass, const TargetRectangle& target_rect, u32 xfb_addr,
                      const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
                      u32 fb_stride, u32 fb_height);
  void DrawRealXFB(VkRenderPass render_pass, const TargetRectangle& target_rect,
                   const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
                   u32 fb_stride, u32 fb_height);

  // Draw the frame, as well as the OSD to the swap chain.
  void DrawScreen(const TargetRectangle& scaled_efb_rect, u32 xfb_addr,
                  const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
                  u32 fb_stride, u32 fb_height);

  // Draw the frame only to the screenshot buffer.
  bool DrawFrameDump(const TargetRectangle& scaled_efb_rect, u32 xfb_addr,
                     const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
                     u32 fb_stride, u32 fb_height, u64 ticks);

  // Sets up renderer state to permit framedumping.
  // Ideally we would have EndFrameDumping be a virtual method of Renderer, but due to various
  // design issues it would have to end up being called in the destructor, which won't work.
  void StartFrameDumping();
  void EndFrameDumping();

  // Fence callback so that we know when frames are ready to be written to the dump.
  // This is done by clearing the fence pointer, so WriteFrameDumpFrame doesn't have to wait.
  void OnFrameDumpImageReady(VkFence fence);

  // Writes the specified buffered frame to the frame dump.
  // NOTE: Assumes that frame.ticks and frame.pending are valid.
  void WriteFrameDumpImage(size_t index);

  // If there is a pending frame in this buffer, writes it to the frame dump.
  // Ensures that the specified readback buffer meets the size requirements of the current frame.
  StagingTexture2D* PrepareFrameDumpImage(u32 width, u32 height, u64 ticks);

  // Ensures all buffered frames are written to frame dump.
  void FlushFrameDump();

  // Copies/scales an image to the currently-bound framebuffer.
  void BlitScreen(VkRenderPass render_pass, const TargetRectangle& dst_rect,
                  const TargetRectangle& src_rect, const Texture2D* src_tex);

  bool ResizeFrameDumpBuffer(u32 new_width, u32 new_height);
  void DestroyFrameDumpResources();

  VkSemaphore m_image_available_semaphore = VK_NULL_HANDLE;
  VkSemaphore m_rendering_finished_semaphore = VK_NULL_HANDLE;

  std::unique_ptr<SwapChain> m_swap_chain;
  std::unique_ptr<BoundingBox> m_bounding_box;
  std::unique_ptr<RasterFont> m_raster_font;

  // Keep a copy of sampler states to avoid cache lookups every draw
  std::array<SamplerState, NUM_PIXEL_SHADER_SAMPLERS> m_sampler_states = {};

  // Shaders used for clear/blit.
  VkShaderModule m_clear_fragment_shader = VK_NULL_HANDLE;

  // Texture used for screenshot/frame dumping
  std::unique_ptr<Texture2D> m_frame_dump_render_texture;
  VkFramebuffer m_frame_dump_framebuffer = VK_NULL_HANDLE;

  // Readback resources for frame dumping
  static const size_t FRAME_DUMP_BUFFERED_FRAMES = 2;
  struct FrameDumpImage
  {
    std::unique_ptr<StagingTexture2D> readback_texture;
    VkFence fence = VK_NULL_HANDLE;
    AVIDump::Frame dump_state = {};
    bool pending = false;
  };
  std::array<FrameDumpImage, FRAME_DUMP_BUFFERED_FRAMES> m_frame_dump_images;
  size_t m_current_frame_dump_image = FRAME_DUMP_BUFFERED_FRAMES - 1;
  bool m_frame_dumping_active = false;
};
}
