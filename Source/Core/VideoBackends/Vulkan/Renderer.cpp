// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>
#include <tuple>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "Core/Core.h"

#include "VideoBackends/Vulkan/BoundingBox.h"
#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PostProcessing.h"
#include "VideoBackends/Vulkan/RasterFont.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

namespace Vulkan
{
Renderer::Renderer(std::unique_ptr<SwapChain> swap_chain)
    : ::Renderer(swap_chain ? static_cast<int>(swap_chain->GetWidth()) : 1,
                 swap_chain ? static_cast<int>(swap_chain->GetHeight()) : 0),
      m_swap_chain(std::move(swap_chain))
{
  UpdateActiveConfig();
  for (size_t i = 0; i < m_sampler_states.size(); i++)
    m_sampler_states[i].hex = RenderState::GetPointSamplerState().hex;
}

Renderer::~Renderer()
{
  UpdateActiveConfig();

  // Ensure all frames are written to frame dump at shutdown.
  if (m_frame_dumping_active)
    EndFrameDumping();

  DestroyFrameDumpResources();
  DestroyShaders();
  DestroySemaphores();
}

Renderer* Renderer::GetInstance()
{
  return static_cast<Renderer*>(g_renderer.get());
}

bool Renderer::Initialize()
{
  BindEFBToStateTracker();

  if (!CreateSemaphores())
  {
    PanicAlert("Failed to create semaphores.");
    return false;
  }

  if (!CompileShaders())
  {
    PanicAlert("Failed to compile shaders.");
    return false;
  }

  m_raster_font = std::make_unique<RasterFont>();
  if (!m_raster_font->Initialize())
  {
    PanicAlert("Failed to initialize raster font.");
    return false;
  }

  m_bounding_box = std::make_unique<BoundingBox>();
  if (!m_bounding_box->Initialize())
  {
    PanicAlert("Failed to initialize bounding box.");
    return false;
  }

  if (g_vulkan_context->SupportsBoundingBox())
  {
    // Bind bounding box to state tracker
    StateTracker::GetInstance()->SetBBoxBuffer(m_bounding_box->GetGPUBuffer(),
                                               m_bounding_box->GetGPUBufferOffset(),
                                               m_bounding_box->GetGPUBufferSize());
  }

  // Initialize post processing.
  m_post_processor = std::make_unique<VulkanPostProcessing>();
  if (!static_cast<VulkanPostProcessing*>(m_post_processor.get())
           ->Initialize(m_raster_font->GetTexture()))
  {
    PanicAlert("failed to initialize post processor.");
    return false;
  }

  // Various initialization routines will have executed commands on the command buffer.
  // Execute what we have done before beginning the first frame.
  g_command_buffer_mgr->PrepareToSubmitCommandBuffer();
  g_command_buffer_mgr->SubmitCommandBuffer(false);
  BeginFrame();

  return true;
}

bool Renderer::CreateSemaphores()
{
  // Create two semaphores, one that is triggered when the swapchain buffer is ready, another after
  // submit and before present
  VkSemaphoreCreateInfo semaphore_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,  // VkStructureType          sType
      nullptr,                                  // const void*              pNext
      0                                         // VkSemaphoreCreateFlags   flags
  };

  VkResult res;
  if ((res = vkCreateSemaphore(g_vulkan_context->GetDevice(), &semaphore_info, nullptr,
                               &m_image_available_semaphore)) != VK_SUCCESS ||
      (res = vkCreateSemaphore(g_vulkan_context->GetDevice(), &semaphore_info, nullptr,
                               &m_rendering_finished_semaphore)) != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateSemaphore failed: ");
    return false;
  }

  return true;
}

void Renderer::DestroySemaphores()
{
  if (m_image_available_semaphore)
  {
    vkDestroySemaphore(g_vulkan_context->GetDevice(), m_image_available_semaphore, nullptr);
    m_image_available_semaphore = VK_NULL_HANDLE;
  }

  if (m_rendering_finished_semaphore)
  {
    vkDestroySemaphore(g_vulkan_context->GetDevice(), m_rendering_finished_semaphore, nullptr);
    m_rendering_finished_semaphore = VK_NULL_HANDLE;
  }
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
  u32 backbuffer_width = m_swap_chain->GetWidth();
  u32 backbuffer_height = m_swap_chain->GetHeight();

  m_raster_font->PrintMultiLineText(m_swap_chain->GetRenderPass(), text,
                                    left * 2.0f / static_cast<float>(backbuffer_width) - 1,
                                    1 - top * 2.0f / static_cast<float>(backbuffer_height),
                                    backbuffer_width, backbuffer_height, color);
}

u32 Renderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data)
{
  if (type == EFBAccessType::PeekColor)
  {
    u32 color = FramebufferManager::GetInstance()->PeekEFBColor(x, y);

    // a little-endian value is expected to be returned
    color = ((color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color << 16) & 0xFF0000));

    // check what to do with the alpha channel (GX_PokeAlphaRead)
    PixelEngine::UPEAlphaReadReg alpha_read_mode = PixelEngine::GetAlphaReadMode();

    if (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24)
    {
      color = RGBA8ToRGBA6ToRGBA8(color);
    }
    else if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
    {
      color = RGBA8ToRGB565ToRGBA8(color);
    }
    if (bpmem.zcontrol.pixel_format != PEControl::RGBA6_Z24)
    {
      color |= 0xFF000000;
    }

    if (alpha_read_mode.ReadMode == 2)
    {
      return color;  // GX_READ_NONE
    }
    else if (alpha_read_mode.ReadMode == 1)
    {
      return color | 0xFF000000;  // GX_READ_FF
    }
    else /*if(alpha_read_mode.ReadMode == 0)*/
    {
      return color & 0x00FFFFFF;  // GX_READ_00
    }
  }
  else  // if (type == EFBAccessType::PeekZ)
  {
    // Depth buffer is inverted for improved precision near far plane
    float depth = 1.0f - FramebufferManager::GetInstance()->PeekEFBDepth(x, y);
    u32 ret = 0;

    if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
    {
      // if Z is in 16 bit format you must return a 16 bit integer
      ret = MathUtil::Clamp<u32>(static_cast<u32>(depth * 65536.0f), 0, 0xFFFF);
    }
    else
    {
      ret = MathUtil::Clamp<u32>(static_cast<u32>(depth * 16777216.0f), 0, 0xFFFFFF);
    }

    return ret;
  }
}

void Renderer::PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
  if (type == EFBAccessType::PokeColor)
  {
    for (size_t i = 0; i < num_points; i++)
    {
      // Convert to expected format (BGRA->RGBA)
      // TODO: Check alpha, depending on mode?
      const EfbPokeData& point = points[i];
      u32 color = ((point.data & 0xFF00FF00) | ((point.data >> 16) & 0xFF) |
                   ((point.data << 16) & 0xFF0000));
      FramebufferManager::GetInstance()->PokeEFBColor(point.x, point.y, color);
    }
  }
  else  // if (type == EFBAccessType::PokeZ)
  {
    for (size_t i = 0; i < num_points; i++)
    {
      // Convert to floating-point depth.
      const EfbPokeData& point = points[i];
      float depth = (1.0f - float(point.data & 0xFFFFFF) / 16777216.0f);
      FramebufferManager::GetInstance()->PokeEFBDepth(point.x, point.y, depth);
    }
  }
}

u16 Renderer::BBoxRead(int index)
{
  s32 value = m_bounding_box->Get(static_cast<size_t>(index));

  // Here we get the min/max value of the truncated position of the upscaled framebuffer.
  // So we have to correct them to the unscaled EFB sizes.
  if (index < 2)
  {
    // left/right
    value = value * EFB_WIDTH / m_target_width;
  }
  else
  {
    // up/down
    value = value * EFB_HEIGHT / m_target_height;
  }

  // fix max values to describe the outer border
  if (index & 1)
    value++;

  return static_cast<u16>(value);
}

void Renderer::BBoxWrite(int index, u16 value)
{
  s32 scaled_value = static_cast<s32>(value);

  // fix max values to describe the outer border
  if (index & 1)
    scaled_value--;

  // scale to internal resolution
  if (index < 2)
  {
    // left/right
    scaled_value = scaled_value * m_target_width / EFB_WIDTH;
  }
  else
  {
    // up/down
    scaled_value = scaled_value * m_target_height / EFB_HEIGHT;
  }

  m_bounding_box->Set(static_cast<size_t>(index), scaled_value);
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
  TargetRectangle result;
  result.left = EFBToScaledX(rc.left);
  result.top = EFBToScaledY(rc.top);
  result.right = EFBToScaledX(rc.right);
  result.bottom = EFBToScaledY(rc.bottom);
  return result;
}

void Renderer::BeginFrame()
{
  // Activate a new command list, and restore state ready for the next draw
  g_command_buffer_mgr->ActivateCommandBuffer();

  // Ensure that the state tracker rebinds everything, and allocates a new set
  // of descriptors out of the next pool.
  StateTracker::GetInstance()->InvalidateDescriptorSets();
  StateTracker::GetInstance()->InvalidateConstants();
  StateTracker::GetInstance()->SetPendingRebind();
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool color_enable, bool alpha_enable,
                           bool z_enable, u32 color, u32 z)
{
  // Native -> EFB coordinates
  TargetRectangle target_rc = Renderer::ConvertEFBRectangle(rc);

  // Size we pass this size to vkBeginRenderPass, it has to be clamped to the framebuffer
  // dimensions. The other backends just silently ignore this case.
  target_rc.ClampUL(0, 0, m_target_width, m_target_height);

  VkRect2D target_vk_rc = {
      {target_rc.left, target_rc.top},
      {static_cast<uint32_t>(target_rc.GetWidth()), static_cast<uint32_t>(target_rc.GetHeight())}};

  // Determine whether the EFB has an alpha channel. If it doesn't, we can clear the alpha
  // channel to 0xFF. This hopefully allows us to use the fast path in most cases.
  if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16 ||
      bpmem.zcontrol.pixel_format == PEControl::RGB8_Z24 ||
      bpmem.zcontrol.pixel_format == PEControl::Z24)
  {
    // Force alpha writes, and clear the alpha channel. This is different to the other backends,
    // where the existing values of the alpha channel are preserved.
    alpha_enable = true;
    color &= 0x00FFFFFF;
  }

  // Convert RGBA8 -> floating-point values.
  VkClearValue clear_color_value = {};
  VkClearValue clear_depth_value = {};
  clear_color_value.color.float32[0] = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
  clear_color_value.color.float32[1] = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
  clear_color_value.color.float32[2] = static_cast<float>((color >> 0) & 0xFF) / 255.0f;
  clear_color_value.color.float32[3] = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
  clear_depth_value.depthStencil.depth = (1.0f - (static_cast<float>(z & 0xFFFFFF) / 16777216.0f));

  // If we're not in a render pass (start of the frame), we can use a clear render pass
  // to discard the data, rather than loading and then clearing.
  bool use_clear_attachments = (color_enable && alpha_enable) || z_enable;
  bool use_clear_render_pass =
      !StateTracker::GetInstance()->InRenderPass() && color_enable && alpha_enable && z_enable;

  // The NVIDIA Vulkan driver causes the GPU to lock up, or throw exceptions if MSAA is enabled,
  // a non-full clear rect is specified, and a clear loadop or vkCmdClearAttachments is used.
  if (g_ActiveConfig.iMultisamples > 1 &&
      DriverDetails::HasBug(DriverDetails::BUG_BROKEN_MSAA_CLEAR))
  {
    use_clear_render_pass = false;
    use_clear_attachments = false;
  }

  // This path cannot be used if the driver implementation doesn't guarantee pixels with no drawn
  // geometry in "this" renderpass won't be cleared
  if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_CLEAR_LOADOP_RENDERPASS))
    use_clear_render_pass = false;

  // Fastest path: Use a render pass to clear the buffers.
  if (use_clear_render_pass)
  {
    VkClearValue clear_values[2] = {clear_color_value, clear_depth_value};
    StateTracker::GetInstance()->BeginClearRenderPass(target_vk_rc, clear_values);
    return;
  }

  // Fast path: Use vkCmdClearAttachments to clear the buffers within a render path
  // We can't use this when preserving alpha but clearing color.
  if (use_clear_attachments)
  {
    VkClearAttachment clear_attachments[2];
    uint32_t num_clear_attachments = 0;
    if (color_enable && alpha_enable)
    {
      clear_attachments[num_clear_attachments].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      clear_attachments[num_clear_attachments].colorAttachment = 0;
      clear_attachments[num_clear_attachments].clearValue = clear_color_value;
      num_clear_attachments++;
      color_enable = false;
      alpha_enable = false;
    }
    if (z_enable)
    {
      clear_attachments[num_clear_attachments].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      clear_attachments[num_clear_attachments].colorAttachment = 0;
      clear_attachments[num_clear_attachments].clearValue = clear_depth_value;
      num_clear_attachments++;
      z_enable = false;
    }
    if (num_clear_attachments > 0)
    {
      VkClearRect vk_rect = {target_vk_rc, 0, FramebufferManager::GetInstance()->GetEFBLayers()};
      if (!StateTracker::GetInstance()->IsWithinRenderArea(
              target_vk_rc.offset.x, target_vk_rc.offset.y, target_vk_rc.extent.width,
              target_vk_rc.extent.height))
      {
        StateTracker::GetInstance()->EndClearRenderPass();
      }
      StateTracker::GetInstance()->BeginRenderPass();

      vkCmdClearAttachments(g_command_buffer_mgr->GetCurrentCommandBuffer(), num_clear_attachments,
                            clear_attachments, 1, &vk_rect);
    }
  }

  // Anything left over for the slow path?
  if (!color_enable && !alpha_enable && !z_enable)
    return;

  // Clearing must occur within a render pass.
  if (!StateTracker::GetInstance()->IsWithinRenderArea(target_vk_rc.offset.x, target_vk_rc.offset.y,
                                                       target_vk_rc.extent.width,
                                                       target_vk_rc.extent.height))
  {
    StateTracker::GetInstance()->EndClearRenderPass();
  }
  StateTracker::GetInstance()->BeginRenderPass();
  StateTracker::GetInstance()->SetPendingRebind();

  // Mask away the appropriate colors and use a shader
  BlendingState blend_state = RenderState::GetNoBlendingBlendState();
  blend_state.colorupdate = color_enable;
  blend_state.alphaupdate = alpha_enable;

  DepthState depth_state = RenderState::GetNoDepthTestingDepthStencilState();
  depth_state.testenable = z_enable;
  depth_state.updateenable = z_enable;
  depth_state.func = ZMode::ALWAYS;

  // No need to start a new render pass, but we do need to restore viewport state
  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD),
                         FramebufferManager::GetInstance()->GetEFBLoadRenderPass(),
                         g_shader_cache->GetPassthroughVertexShader(),
                         g_shader_cache->GetPassthroughGeometryShader(), m_clear_fragment_shader);

  draw.SetMultisamplingState(FramebufferManager::GetInstance()->GetEFBMultisamplingState());
  draw.SetDepthState(depth_state);
  draw.SetBlendState(blend_state);

  draw.DrawColoredQuad(target_rc.left, target_rc.top, target_rc.GetWidth(), target_rc.GetHeight(),
                       clear_color_value.color.float32[0], clear_color_value.color.float32[1],
                       clear_color_value.color.float32[2], clear_color_value.color.float32[3],
                       clear_depth_value.depthStencil.depth);
}

void Renderer::SkipClearScreen(bool colorEnable, bool alphaEnable, bool zEnable)
{
  // todo
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->SetPendingRebind();
  FramebufferManager::GetInstance()->ReinterpretPixelData(convtype);

  // EFB framebuffer has now changed, so update accordingly.
  BindEFBToStateTracker();
}

void Renderer::AsyncTimewarpDraw()
{
  // TODO: D3D12 Asynchronous Timewarp
}

void Renderer::SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height,
                        const EFBRectangle& rc, u64 ticks, float gamma)
{
  // Pending/batched EFB pokes should be included in the final image.
  FramebufferManager::GetInstance()->FlushEFBPokes();

  // Check that we actually have an image to render in XFB-on modes.
  if ((!m_xfb_written && !g_ActiveConfig.RealXFBEnabled()) || !fb_width || !fb_height)
  {
    Core::Callback_VideoCopiedToXFB(false);
    return;
  }
  u32 xfb_count = 0;
  const XFBSourceBase* const* xfb_sources =
      FramebufferManager::GetXFBSource(xfb_addr, fb_stride, fb_height, &xfb_count);
  if (g_ActiveConfig.VirtualXFBEnabled() && (!xfb_sources || xfb_count == 0))
  {
    Core::Callback_VideoCopiedToXFB(false);
    return;
  }

  // End the current render pass.
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->OnEndFrame();

  // There are a few variables which can alter the final window draw rectangle, and some of them
  // are determined by guest state. Currently, the only way to catch these is to update every frame.
  UpdateDrawRectangle();

  // Scale the source rectangle to the internal resolution when XFB is disabled.
  TargetRectangle scaled_efb_rect = Renderer::ConvertEFBRectangle(rc);

  // If MSAA is enabled, and we're not using XFB, we need to resolve the EFB framebuffer before
  // rendering the final image to the screen, or dumping the frame. This is because we can't resolve
  // an image within a render pass, which will have already started by the time it is used.
  TransitionBuffersForSwap(scaled_efb_rect, xfb_sources, xfb_count);

  // Render the frame dump image if enabled.
  if (IsFrameDumping())
  {
    // If we haven't dumped a single frame yet, set up frame dumping.
    if (!m_frame_dumping_active)
      StartFrameDumping();

    DrawFrameDump(scaled_efb_rect, xfb_addr, xfb_sources, xfb_count, fb_width, fb_stride, fb_height,
                  ticks);
  }
  else
  {
    // If frame dumping was previously enabled, flush all frames and remove the fence callback.
    if (m_frame_dumping_active)
      EndFrameDumping();
  }

  // Ensure the worker thread is not still submitting a previous command buffer.
  // In other words, the last frame has been submitted (otherwise the next call would
  // be a race, as the image may not have been consumed yet).
  g_command_buffer_mgr->PrepareToSubmitCommandBuffer();

  // Draw to the screen if we have a swap chain.
  if (m_swap_chain)
  {
    DrawScreen(scaled_efb_rect, xfb_addr, xfb_sources, xfb_count, fb_width, fb_stride, fb_height);

    // Submit the current command buffer, signaling rendering finished semaphore when it's done
    // Because this final command buffer is rendering to the swap chain, we need to wait for
    // the available semaphore to be signaled before executing the buffer. This final submission
    // can happen off-thread in the background while we're preparing the next frame.
    g_command_buffer_mgr->SubmitCommandBuffer(
        true, m_image_available_semaphore, m_rendering_finished_semaphore,
        m_swap_chain->GetSwapChain(), m_swap_chain->GetCurrentImageIndex());
  }
  else
  {
    // No swap chain, just execute command buffer.
    g_command_buffer_mgr->SubmitCommandBuffer(true);
  }

  // NOTE: It is important that no rendering calls are made to the EFB between submitting the
  // (now-previous) frame and after the below config checks are completed. If the target size
  // changes, as the resize methods to not defer the destruction of the framebuffer, the current
  // command buffer will contain references to a now non-existent framebuffer.

  // Prep for the next frame (get command buffer ready) before doing anything else.
  BeginFrame();

  // Restore the EFB color texture to color attachment ready for rendering the next frame.
  FramebufferManager::GetInstance()->GetEFBColorTexture()->TransitionToLayout(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Determine what (if anything) has changed in the config.
  CheckForConfigChanges();

  // Handle host window resizes.
  CheckForSurfaceChange();

  // Handle output size changes from the guest.
  // There is a downside to doing this here is that if the game changes its XFB source area,
  // the changes will be delayed by one frame. For the moment it has to be done here because
  // this can cause a target size change, which would result in a black frame if done earlier.
  CheckForTargetResize(fb_width, fb_stride, fb_height);

  // Update the window size based on the frame that was just rendered.
  // Due to depending on guest state, we need to call this every frame.
  SetWindowSize(static_cast<int>(fb_stride), static_cast<int>(fb_height));

  // Clean up stale textures.
  TextureCache::GetInstance()->Cleanup(frameCount);

  // Pull in now-ready async shaders.
  g_shader_cache->RetrieveAsyncShaders();
}

void Renderer::TransitionBuffersForSwap(const TargetRectangle& scaled_rect,
                                        const XFBSourceBase* const* xfb_sources, u32 xfb_count)
{
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();

  if (!g_ActiveConfig.bUseXFB)
  {
    // Drawing EFB direct.
    if (g_ActiveConfig.iMultisamples > 1)
    {
      // While the source rect can be out-of-range when drawing, the resolve rectangle must be
      // within the bounds of the texture.
      VkRect2D region = {
          {scaled_rect.left, scaled_rect.top},
          {static_cast<u32>(scaled_rect.GetWidth()), static_cast<u32>(scaled_rect.GetHeight())}};
      region = Util::ClampRect2D(region, FramebufferManager::GetInstance()->GetEFBWidth(),
                                 FramebufferManager::GetInstance()->GetEFBHeight());

      Vulkan::Texture2D* rtex = FramebufferManager::GetInstance()->ResolveEFBColorTexture(region);
      rtex->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    else
    {
      FramebufferManager::GetInstance()->GetEFBColorTexture()->TransitionToLayout(
          command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    return;
  }

  // Drawing XFB sources, so transition all of them.
  // Don't need the EFB, so leave it as-is.
  for (u32 i = 0; i < xfb_count; i++)
  {
    const XFBSource* xfb_source = static_cast<const XFBSource*>(xfb_sources[i]);
    xfb_source->GetTexture()->GetRawTexIdentifier()->TransitionToLayout(
        command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
}

void Renderer::DrawFrame(VkRenderPass render_pass, const TargetRectangle& target_rect,
                         const TargetRectangle& scaled_efb_rect, u32 xfb_addr,
                         const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
                         u32 fb_stride, u32 fb_height)
{
  if (!g_ActiveConfig.bUseXFB)
    DrawEFB(render_pass, target_rect, scaled_efb_rect);
  else if (!g_ActiveConfig.bUseRealXFB)
    DrawVirtualXFB(render_pass, target_rect, xfb_addr, xfb_sources, xfb_count, fb_width, fb_stride,
                   fb_height);
  else
    DrawRealXFB(render_pass, target_rect, xfb_sources, xfb_count, fb_width, fb_stride, fb_height);
}

void Renderer::DrawEFB(VkRenderPass render_pass, const TargetRectangle& target_rect,
                       const TargetRectangle& scaled_efb_rect)
{
  // Transition the EFB render target to a shader resource.
  Texture2D* efb_color_texture =
      g_ActiveConfig.iMultisamples > 1 ?
          FramebufferManager::GetInstance()->GetResolvedEFBColorTexture() :
          FramebufferManager::GetInstance()->GetEFBColorTexture();

  // Copy EFB -> backbuffer
  BlitScreen(render_pass, target_rect, scaled_efb_rect, efb_color_texture);
}

void Renderer::DrawVirtualXFB(VkRenderPass render_pass, const TargetRectangle& target_rect,
                              u32 xfb_addr, const XFBSourceBase* const* xfb_sources, u32 xfb_count,
                              u32 fb_width, u32 fb_stride, u32 fb_height)
{
  for (u32 i = 0; i < xfb_count; ++i)
  {
    const XFBSource* xfb_source = static_cast<const XFBSource*>(xfb_sources[i]);
    TargetRectangle source_rect = xfb_source->sourceRc;
    TargetRectangle draw_rect;

    int xfb_width = static_cast<int>(xfb_source->srcWidth);
    int xfb_height = static_cast<int>(xfb_source->srcHeight);
    int h_offset = (static_cast<s32>(xfb_source->srcAddr) - static_cast<s32>(xfb_addr)) /
                   (static_cast<s32>(fb_stride) * 2);
    draw_rect.top =
        target_rect.top + h_offset * target_rect.GetHeight() / static_cast<s32>(fb_height);
    draw_rect.bottom = target_rect.top + (h_offset + xfb_height) * target_rect.GetHeight() /
                                             static_cast<s32>(fb_height);
    draw_rect.left =
        target_rect.left + (target_rect.GetWidth() -
                            xfb_width * target_rect.GetWidth() / static_cast<s32>(fb_stride)) /
                               2;
    draw_rect.right =
        target_rect.left + (target_rect.GetWidth() +
                            xfb_width * target_rect.GetWidth() / static_cast<s32>(fb_stride)) /
                               2;

    source_rect.right -= Renderer::EFBToScaledX(fb_stride - fb_width);
    BlitScreen(render_pass, draw_rect, source_rect,
               xfb_source->GetTexture()->GetRawTexIdentifier());
  }
}

void Renderer::DrawRealXFB(VkRenderPass render_pass, const TargetRectangle& target_rect,
                           const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
                           u32 fb_stride, u32 fb_height)
{
  for (u32 i = 0; i < xfb_count; ++i)
  {
    const XFBSource* xfb_source = static_cast<const XFBSource*>(xfb_sources[i]);
    TargetRectangle source_rect = xfb_source->sourceRc;
    TargetRectangle draw_rect = target_rect;
    source_rect.right -= fb_stride - fb_width;
    BlitScreen(render_pass, draw_rect, source_rect,
               xfb_source->GetTexture()->GetRawTexIdentifier());
  }
}

void Renderer::DrawScreen(const TargetRectangle& scaled_efb_rect, u32 xfb_addr,
                          const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
                          u32 fb_stride, u32 fb_height)
{
  VkResult res;
  if (!g_command_buffer_mgr->CheckLastPresentFail())
  {
    // Grab the next image from the swap chain in preparation for drawing the window.
    res = m_swap_chain->AcquireNextImage(m_image_available_semaphore);
  }
  else
  {
    // If the last present failed, we need to recreate the swap chain.
    res = VK_ERROR_OUT_OF_DATE_KHR;
  }

  if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
  {
    // There's an issue here. We can't resize the swap chain while the GPU is still busy with it,
    // but calling WaitForGPUIdle would create a deadlock as PrepareToSubmitCommandBuffer has been
    // called by SwapImpl. WaitForGPUIdle waits on the semaphore, which PrepareToSubmitCommandBuffer
    // has already done, so it blocks indefinitely. To work around this, we submit the current
    // command buffer, resize the swap chain (which calls WaitForGPUIdle), and then finally call
    // PrepareToSubmitCommandBuffer to return to the state that the caller expects.
    g_command_buffer_mgr->SubmitCommandBuffer(false);
    m_swap_chain->ResizeSwapChain();
    BeginFrame();
    g_command_buffer_mgr->PrepareToSubmitCommandBuffer();
    res = m_swap_chain->AcquireNextImage(m_image_available_semaphore);
  }
  if (res != VK_SUCCESS)
    PanicAlert("Failed to grab image from swap chain");

  // Transition from undefined (or present src, but it can be substituted) to
  // color attachment ready for writing. These transitions must occur outside
  // a render pass, unless the render pass declares a self-dependency.
  Texture2D* backbuffer = m_swap_chain->GetCurrentTexture();
  backbuffer->OverrideImageLayout(VK_IMAGE_LAYOUT_UNDEFINED);
  backbuffer->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Begin render pass for rendering to the swap chain.
  VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  VkRenderPassBeginInfo info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                nullptr,
                                m_swap_chain->GetRenderPass(),
                                m_swap_chain->GetCurrentFramebuffer(),
                                {{0, 0}, {backbuffer->GetWidth(), backbuffer->GetHeight()}},
                                1,
                                &clear_value};
  vkCmdBeginRenderPass(g_command_buffer_mgr->GetCurrentCommandBuffer(), &info,
                       VK_SUBPASS_CONTENTS_INLINE);

  // Draw guest buffers (EFB or XFB)
  DrawFrame(m_swap_chain->GetRenderPass(), GetTargetRectangle(), scaled_efb_rect, xfb_addr,
            xfb_sources, xfb_count, fb_width, fb_stride, fb_height);

  // Draw OSD
  Util::SetViewportAndScissor(g_command_buffer_mgr->GetCurrentCommandBuffer(), 0, 0,
                              backbuffer->GetWidth(), backbuffer->GetHeight());
  DrawDebugText();
  OSD::DoCallbacks(OSD::CallbackType::OnFrame);
  OSD::DrawMessages();

  // End drawing to backbuffer
  vkCmdEndRenderPass(g_command_buffer_mgr->GetCurrentCommandBuffer());

  // Transition the backbuffer to PRESENT_SRC to ensure all commands drawing
  // to it have finished before present.
  backbuffer->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

bool Renderer::DrawFrameDump(const TargetRectangle& scaled_efb_rect, u32 xfb_addr,
                             const XFBSourceBase* const* xfb_sources, u32 xfb_count, u32 fb_width,
                             u32 fb_stride, u32 fb_height, u64 ticks)
{
  TargetRectangle target_rect = CalculateFrameDumpDrawRectangle();
  u32 width = std::max(1u, static_cast<u32>(target_rect.GetWidth()));
  u32 height = std::max(1u, static_cast<u32>(target_rect.GetHeight()));
  if (!ResizeFrameDumpBuffer(width, height))
    return false;

  // If there was a previous frame dumped, we'll still be in TRANSFER_SRC layout.
  m_frame_dump_render_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  VkClearRect clear_rect = {{{0, 0}, {width, height}}, 0, 1};
  VkClearAttachment clear_attachment = {VK_IMAGE_ASPECT_COLOR_BIT, 0, clear_value};
  VkRenderPassBeginInfo info = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      nullptr,
      FramebufferManager::GetInstance()->GetColorCopyForReadbackRenderPass(),
      m_frame_dump_framebuffer,
      {{0, 0}, {width, height}},
      1,
      &clear_value};
  vkCmdBeginRenderPass(g_command_buffer_mgr->GetCurrentCommandBuffer(), &info,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdClearAttachments(g_command_buffer_mgr->GetCurrentCommandBuffer(), 1, &clear_attachment, 1,
                        &clear_rect);
  DrawFrame(FramebufferManager::GetInstance()->GetColorCopyForReadbackRenderPass(), target_rect,
            scaled_efb_rect, xfb_addr, xfb_sources, xfb_count, fb_width, fb_stride, fb_height);
  vkCmdEndRenderPass(g_command_buffer_mgr->GetCurrentCommandBuffer());

  // Prepare the readback texture for copying.
  StagingTexture2D* readback_texture = PrepareFrameDumpImage(width, height, ticks);
  if (!readback_texture)
    return false;

  // Queue a copy to the current frame dump buffer. It will be written to the frame dump later.
  m_frame_dump_render_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  readback_texture->CopyFromImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  m_frame_dump_render_texture->GetImage(),
                                  VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, width, height, 0, 0);
  return true;
}

void Renderer::StartFrameDumping()
{
  _assert_(!m_frame_dumping_active);

  // Register fence callback so that we know when frames are ready to be written to the dump.
  // This is done by clearing the fence pointer, so WriteFrameDumpFrame doesn't have to wait.
  auto queued_callback = [](VkCommandBuffer, VkFence) {};
  auto signaled_callback = std::bind(&Renderer::OnFrameDumpImageReady, this, std::placeholders::_1);

  // We use the array pointer as a key here, that way if Renderer needed fence callbacks in
  // the future it could be used without conflicting.
  // We're not interested in when fences are submitted, so the first callback is a no-op.
  g_command_buffer_mgr->AddFencePointCallback(
      m_frame_dump_images.data(), std::move(queued_callback), std::move(signaled_callback));
  m_frame_dumping_active = true;
}

void Renderer::EndFrameDumping()
{
  _assert_(m_frame_dumping_active);

  // Write any pending frames to the frame dump.
  FlushFrameDump();

  // Remove the fence callback that we registered earlier, one less function that needs to be
  // called when preparing a command buffer.
  g_command_buffer_mgr->RemoveFencePointCallback(m_frame_dump_images.data());
  m_frame_dumping_active = false;
}

void Renderer::OnFrameDumpImageReady(VkFence fence)
{
  for (FrameDumpImage& frame : m_frame_dump_images)
  {
    // fence being a null handle means that we don't have to wait to re-use this image.
    if (frame.fence == fence)
      frame.fence = VK_NULL_HANDLE;
  }
}

void Renderer::WriteFrameDumpImage(size_t index)
{
  FrameDumpImage& frame = m_frame_dump_images[index];
  _assert_(frame.pending);

  // Check fence has been signaled.
  // The callback here should set fence to null.
  if (frame.fence != VK_NULL_HANDLE)
  {
    g_command_buffer_mgr->WaitForFence(frame.fence);
    _assert_(frame.fence == VK_NULL_HANDLE);
  }

  // Copy the now-populated image data to the output file.
  DumpFrameData(reinterpret_cast<const u8*>(frame.readback_texture->GetMapPointer()),
                static_cast<int>(frame.readback_texture->GetWidth()),
                static_cast<int>(frame.readback_texture->GetHeight()),
                static_cast<int>(frame.readback_texture->GetRowStride()), frame.dump_state);

  frame.pending = false;
}

StagingTexture2D* Renderer::PrepareFrameDumpImage(u32 width, u32 height, u64 ticks)
{
  // Ensure the last frame that was sent to the frame dump has completed encoding before we send
  // the next image to it.
  FinishFrameData();

  // If the last image hasn't been written to the frame dump yet, write it now.
  // This is necessary so that the worker thread is no more than one frame behind, and the pointer
  // (which is actually the buffer) is safe for us to re-use next time.
  if (m_frame_dump_images[m_current_frame_dump_image].pending)
    WriteFrameDumpImage(m_current_frame_dump_image);

  // Move to the next image buffer
  m_current_frame_dump_image = (m_current_frame_dump_image + 1) % FRAME_DUMP_BUFFERED_FRAMES;
  FrameDumpImage& image = m_frame_dump_images[m_current_frame_dump_image];

  // Ensure the dimensions of the readback texture are sufficient.
  if (!image.readback_texture || width != image.readback_texture->GetWidth() ||
      height != image.readback_texture->GetHeight())
  {
    // Allocate a new readback texture.
    // The reset() call is here so that the memory is released before allocating the new texture.
    image.readback_texture.reset();
    image.readback_texture = StagingTexture2D::Create(STAGING_BUFFER_TYPE_READBACK, width, height,
                                                      EFB_COLOR_TEXTURE_FORMAT);

    if (!image.readback_texture || !image.readback_texture->Map())
    {
      // Not actually fatal, just means we can't dump this frame.
      PanicAlert("Failed to allocate frame dump readback texture.");
      image.readback_texture.reset();
      return nullptr;
    }
  }

  // The copy happens immediately after this function returns, so flag this frame as pending.
  image.fence = g_command_buffer_mgr->GetCurrentCommandBufferFence();
  image.dump_state = AVIDump::FetchState(ticks);
  image.pending = true;
  return image.readback_texture.get();
}

void Renderer::FlushFrameDump()
{
  // We must write frames in order, so this is why we use a counter rather than a range.
  for (size_t i = 0; i < FRAME_DUMP_BUFFERED_FRAMES; i++)
  {
    if (m_frame_dump_images[m_current_frame_dump_image].pending)
      WriteFrameDumpImage(m_current_frame_dump_image);

    m_current_frame_dump_image = (m_current_frame_dump_image + 1) % FRAME_DUMP_BUFFERED_FRAMES;
  }

  // Since everything has been written now, may as well start at index zero.
  // count-1 here because the index is incremented before usage.
  m_current_frame_dump_image = FRAME_DUMP_BUFFERED_FRAMES - 1;
}

void Renderer::BlitScreen(VkRenderPass render_pass, const TargetRectangle& dst_rect,
                          const TargetRectangle& src_rect, const Texture2D* src_tex)
{
  VulkanPostProcessing* post_processor = static_cast<VulkanPostProcessing*>(m_post_processor.get());
  if (g_ActiveConfig.iStereoMode == STEREO_SBS || g_ActiveConfig.iStereoMode == STEREO_TAB)
  {
    TargetRectangle left_rect;
    TargetRectangle right_rect;
    std::tie(left_rect, right_rect) = ConvertStereoRectangle(dst_rect);

    post_processor->BlitFromTexture(left_rect, src_rect, src_tex, 0, render_pass);
    post_processor->BlitFromTexture(right_rect, src_rect, src_tex, 1, render_pass);
  }
  else if (g_ActiveConfig.iStereoMode == STEREO_QUADBUFFER)
  {
    post_processor->BlitFromTexture(dst_rect, src_rect, src_tex, -1, render_pass);
  }
  else
  {
    post_processor->BlitFromTexture(dst_rect, src_rect, src_tex, 0, render_pass);
  }
}

bool Renderer::ResizeFrameDumpBuffer(u32 new_width, u32 new_height)
{
  if (m_frame_dump_render_texture && m_frame_dump_render_texture->GetWidth() == new_width &&
      m_frame_dump_render_texture->GetHeight() == new_height)
  {
    return true;
  }

  // Ensure all previous frames have been dumped, since we are destroying a framebuffer
  // that may still be in use.
  FlushFrameDump();

  if (m_frame_dump_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_frame_dump_framebuffer, nullptr);
    m_frame_dump_framebuffer = VK_NULL_HANDLE;
  }

  m_frame_dump_render_texture =
      Texture2D::Create(new_width, new_height, 1, 1, EFB_COLOR_TEXTURE_FORMAT,
                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

  if (!m_frame_dump_render_texture)
  {
    WARN_LOG(VIDEO, "Failed to resize frame dump render texture");
    m_frame_dump_render_texture.reset();
    return false;
  }

  VkImageView attachment = m_frame_dump_render_texture->GetView();
  VkFramebufferCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  info.renderPass = FramebufferManager::GetInstance()->GetColorCopyForReadbackRenderPass();
  info.attachmentCount = 1;
  info.pAttachments = &attachment;
  info.width = new_width;
  info.height = new_height;
  info.layers = 1;

  VkResult res =
      vkCreateFramebuffer(g_vulkan_context->GetDevice(), &info, nullptr, &m_frame_dump_framebuffer);
  if (res != VK_SUCCESS)
  {
    WARN_LOG(VIDEO, "Failed to create frame dump framebuffer");
    m_frame_dump_render_texture.reset();
    return false;
  }

  // Render pass expects texture is in transfer src to start with.
  m_frame_dump_render_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  return true;
}

void Renderer::DestroyFrameDumpResources()
{
  if (m_frame_dump_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_frame_dump_framebuffer, nullptr);
    m_frame_dump_framebuffer = VK_NULL_HANDLE;
  }

  m_frame_dump_render_texture.reset();

  for (FrameDumpImage& image : m_frame_dump_images)
  {
    image.readback_texture.reset();
    image.fence = VK_NULL_HANDLE;
    image.dump_state = {};
    image.pending = false;
  }
  m_current_frame_dump_image = FRAME_DUMP_BUFFERED_FRAMES - 1;
}

void Renderer::CheckForTargetResize(u32 fb_width, u32 fb_stride, u32 fb_height)
{
  if (FramebufferManagerBase::LastXfbWidth() == fb_stride &&
      FramebufferManagerBase::LastXfbHeight() == fb_height)
  {
    return;
  }

  u32 new_width = (fb_stride < 1 || fb_stride > MAX_XFB_WIDTH) ? MAX_XFB_WIDTH : fb_stride;
  u32 new_height = (fb_height < 1 || fb_height > MAX_XFB_HEIGHT) ? MAX_XFB_HEIGHT : fb_height;
  FramebufferManagerBase::SetLastXfbWidth(new_width);
  FramebufferManagerBase::SetLastXfbHeight(new_height);

  // Changing the XFB source area may alter the target size.
  if (CalculateTargetSize())
    ResizeEFBTextures();
}

void Renderer::CheckForSurfaceChange()
{
  if (!m_surface_needs_change.IsSet())
    return;

  // Wait for the GPU to catch up since we're going to destroy the swap chain.
  g_command_buffer_mgr->WaitForGPUIdle();

  // Clear the present failed flag, since we don't want to resize after recreating.
  g_command_buffer_mgr->CheckLastPresentFail();

  // Fast path, if the surface handle is the same, the window has just been resized.
  if (m_swap_chain && m_new_surface_handle == m_swap_chain->GetNativeHandle())
  {
    INFO_LOG(VIDEO, "Detected window resize.");
    m_swap_chain->RecreateSwapChain();

    // Notify the main thread we are done.
    m_surface_needs_change.Clear();
    m_new_surface_handle = nullptr;
    m_surface_changed.Set();
  }
  else
  {
    // Did we previously have a swap chain?
    if (m_swap_chain)
    {
      if (!m_new_surface_handle)
      {
        // If there is no surface now, destroy the swap chain.
        m_swap_chain.reset();
      }
      else
      {
        // Recreate the surface. If this fails we're in trouble.
        if (!m_swap_chain->RecreateSurface(m_new_surface_handle))
          PanicAlert("Failed to recreate Vulkan surface. Cannot continue.");
      }
    }
    else
    {
      // Previously had no swap chain. So create one.
      VkSurfaceKHR surface = SwapChain::CreateVulkanSurface(g_vulkan_context->GetVulkanInstance(),
                                                            m_new_surface_handle);
      if (surface != VK_NULL_HANDLE)
      {
        m_swap_chain = SwapChain::Create(m_new_surface_handle, surface, g_ActiveConfig.IsVSync());
        if (!m_swap_chain)
          PanicAlert("Failed to create swap chain.");
      }
      else
      {
        PanicAlert("Failed to create surface.");
      }
    }

    // Notify calling thread.
    m_surface_needs_change.Clear();
    m_new_surface_handle = nullptr;
    m_surface_changed.Set();
  }

  // Handle case where the dimensions are now different.
  OnSwapChainResized();
}

void Renderer::CheckForConfigChanges()
{
  // Save the video config so we can compare against to determine which settings have changed.
  int old_anisotropy = g_ActiveConfig.iMaxAnisotropy;
  int old_aspect_ratio = g_ActiveConfig.iAspectRatio;
  int old_efb_scale = g_ActiveConfig.iEFBScale;
  bool old_force_filtering = g_ActiveConfig.bForceFiltering;
  bool old_use_xfb = g_ActiveConfig.bUseXFB;
  bool old_use_realxfb = g_ActiveConfig.bUseRealXFB;

  // Copy g_Config to g_ActiveConfig.
  // NOTE: This can potentially race with the UI thread, however if it does, the changes will be
  // delayed until the next time CheckForConfigChanges is called.
  UpdateActiveConfig();

  // Determine which (if any) settings have changed.
  bool anisotropy_changed = old_anisotropy != g_ActiveConfig.iMaxAnisotropy;
  bool force_texture_filtering_changed = old_force_filtering != g_ActiveConfig.bForceFiltering;
  bool efb_scale_changed = old_efb_scale != g_ActiveConfig.iEFBScale;
  bool aspect_changed = old_aspect_ratio != g_ActiveConfig.iAspectRatio;
  bool use_xfb_changed = old_use_xfb != g_ActiveConfig.bUseXFB;
  bool use_realxfb_changed = old_use_realxfb != g_ActiveConfig.bUseRealXFB;

  // Update texture cache settings with any changed options.
  TextureCache::GetInstance()->OnConfigChanged(g_ActiveConfig);

  // Handle settings that can cause the target rectangle to change.
  if (efb_scale_changed || aspect_changed || use_xfb_changed || use_realxfb_changed)
  {
    if (CalculateTargetSize())
      ResizeEFBTextures();
  }

  // MSAA samples changed, we need to recreate the EFB render pass.
  // If the stereoscopy mode changed, we need to recreate the buffers as well.
  // SSAA changed on/off, we have to recompile shaders.
  // Changing stereoscopy from off<->on also requires shaders to be recompiled.
  if (CheckForHostConfigChanges())
  {
    g_command_buffer_mgr->WaitForGPUIdle();
    FramebufferManager::GetInstance()->RecreateRenderPass();
    FramebufferManager::GetInstance()->ResizeEFBTextures();
    BindEFBToStateTracker();
    RecompileShaders();
    FramebufferManager::GetInstance()->RecompileShaders();
    g_shader_cache->ReloadShaderAndPipelineCaches();
    g_shader_cache->RecompileSharedShaders();
    StateTracker::GetInstance()->InvalidateShaderPointers();
    StateTracker::GetInstance()->ReloadPipelineUIDCache();
  }

  // For vsync, we need to change the present mode, which means recreating the swap chain.
  if (m_swap_chain && g_ActiveConfig.IsVSync() != m_swap_chain->IsVSyncEnabled())
  {
    g_command_buffer_mgr->WaitForGPUIdle();
    m_swap_chain->SetVSync(g_ActiveConfig.IsVSync());
  }

  // For quad-buffered stereo we need to change the layer count, so recreate the swap chain.
  if (m_swap_chain &&
      (g_ActiveConfig.iStereoMode == STEREO_QUADBUFFER) != m_swap_chain->IsStereoEnabled())
  {
    g_command_buffer_mgr->WaitForGPUIdle();
    m_swap_chain->RecreateSwapChain();
  }

  // Wipe sampler cache if force texture filtering or anisotropy changes.
  if (anisotropy_changed || force_texture_filtering_changed)
    ResetSamplerStates();

  // Check for a changed post-processing shader and recompile if needed.
  static_cast<VulkanPostProcessing*>(m_post_processor.get())->UpdateConfig();
}

void Renderer::OnSwapChainResized()
{
  m_backbuffer_width = m_swap_chain->GetWidth();
  m_backbuffer_height = m_swap_chain->GetHeight();
  UpdateDrawRectangle();
  if (CalculateTargetSize())
    ResizeEFBTextures();
}

void Renderer::BindEFBToStateTracker()
{
  // Update framebuffer in state tracker
  VkRect2D framebuffer_size = {{0, 0},
                               {FramebufferManager::GetInstance()->GetEFBWidth(),
                                FramebufferManager::GetInstance()->GetEFBHeight()}};
  StateTracker::GetInstance()->SetRenderPass(
      FramebufferManager::GetInstance()->GetEFBLoadRenderPass(),
      FramebufferManager::GetInstance()->GetEFBClearRenderPass());
  StateTracker::GetInstance()->SetFramebuffer(
      FramebufferManager::GetInstance()->GetEFBFramebuffer(), framebuffer_size);
  StateTracker::GetInstance()->SetMultisamplingstate(
      FramebufferManager::GetInstance()->GetEFBMultisamplingState());
}

void Renderer::ResizeEFBTextures()
{
  // Ensure the GPU is finished with the current EFB textures.
  g_command_buffer_mgr->WaitForGPUIdle();
  FramebufferManager::GetInstance()->ResizeEFBTextures();
  BindEFBToStateTracker();

  // Viewport and scissor rect have to be reset since they will be scaled differently.
  SetViewport();
  BPFunctions::SetScissor();
}

void Renderer::ApplyState()
{
}

void Renderer::ResetAPIState()
{
  // End the EFB render pass if active
  StateTracker::GetInstance()->EndRenderPass();
}

void Renderer::RestoreAPIState()
{
  // Instruct the state tracker to re-bind everything before the next draw
  StateTracker::GetInstance()->SetPendingRebind();
}

void Renderer::SetRasterizationState(const RasterizationState& state)
{
  StateTracker::GetInstance()->SetRasterizationState(state);
}

void Renderer::SetDepthState(const DepthState& state)
{
  StateTracker::GetInstance()->SetDepthState(state);
}

void Renderer::SetBlendingState(const BlendingState& state)
{
  StateTracker::GetInstance()->SetBlendState(state);
}

void Renderer::SetSamplerState(u32 index, const SamplerState& state)
{
  // Skip lookup if the state hasn't changed.
  if (m_sampler_states[index].hex == state.hex)
    return;

  // Look up new state and replace in state tracker.
  VkSampler sampler = g_object_cache->GetSampler(state);
  if (sampler == VK_NULL_HANDLE)
  {
    ERROR_LOG(VIDEO, "Failed to create sampler");
    sampler = g_object_cache->GetPointSampler();
  }

  StateTracker::GetInstance()->SetSampler(index, sampler);
  m_sampler_states[index].hex = state.hex;
}

void Renderer::ResetSamplerStates()
{
  // Ensure none of the sampler objects are in use.
  // This assumes that none of the samplers are in use on the command list currently being recorded.
  g_command_buffer_mgr->WaitForGPUIdle();

  // Invalidate all sampler states, next draw will re-initialize them.
  for (size_t i = 0; i < m_sampler_states.size(); i++)
  {
    m_sampler_states[i].hex = RenderState::GetPointSamplerState().hex;
    StateTracker::GetInstance()->SetSampler(i, g_object_cache->GetPointSampler());
  }

  // Invalidate all sampler objects (some will be unused now).
  g_object_cache->ClearSamplerCache();
}

void Renderer::SetInterlacingMode()
{
}

void Renderer::SetScissorRect(const EFBRectangle& rc)
{
  TargetRectangle target_rc = ConvertEFBRectangle(rc);

  VkRect2D scissor = {
      {target_rc.left, target_rc.top},
      {static_cast<uint32_t>(target_rc.GetWidth()), static_cast<uint32_t>(target_rc.GetHeight())}};

  StateTracker::GetInstance()->SetScissor(scissor);
}

void Renderer::SetViewport()
{
  int scissor_x_offset = bpmem.scissorOffset.x * 2;
  int scissor_y_offset = bpmem.scissorOffset.y * 2;

  float x = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - scissor_x_offset);
  float y = Renderer::EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - scissor_y_offset);
  float width = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
  float height = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);
  float min_depth = (xfmem.viewport.farZ - xfmem.viewport.zRange) / 16777216.0f;
  float max_depth = xfmem.viewport.farZ / 16777216.0f;
  if (width < 0.0f)
  {
    x += width;
    width = -width;
  }
  if (height < 0.0f)
  {
    y += height;
    height = -height;
  }

  // If an oversized or inverted depth range is used, we need to calculate the depth range in the
  // vertex shader.
  // TODO: Inverted depth ranges are bugged in all drivers, which should be added to DriverDetails.
  if (UseVertexDepthRange())
  {
    // We need to ensure depth values are clamped the maximum value supported by the console GPU.
    min_depth = 0.0f;
    max_depth = GX_MAX_DEPTH;
  }

  // We use an inverted depth range here to apply the Reverse Z trick.
  // This trick makes sure we match the precision provided by the 1:0
  // clipping depth range on the hardware.
  VkViewport viewport = {x, y, width, height, 1.0f - max_depth, 1.0f - min_depth};
  StateTracker::GetInstance()->SetViewport(viewport);
}

void Renderer::ChangeSurface(void* new_surface_handle)
{
  // Called by the main thread when the window is resized.
  m_new_surface_handle = new_surface_handle;
  m_surface_needs_change.Set();
  m_surface_changed.Set();
}

void Renderer::RecompileShaders()
{
  DestroyShaders();
  if (!CompileShaders())
    PanicAlert("Failed to recompile shaders.");
}

bool Renderer::CompileShaders()
{
  static const char CLEAR_FRAGMENT_SHADER_SOURCE[] = R"(
    layout(location = 0) in float3 uv0;
    layout(location = 1) in float4 col0;
    layout(location = 0) out float4 ocol0;

    void main()
    {
      ocol0 = col0;
    }

  )";

  std::string source = g_shader_cache->GetUtilityShaderHeader() + CLEAR_FRAGMENT_SHADER_SOURCE;
  m_clear_fragment_shader = Util::CompileAndCreateFragmentShader(source);

  return m_clear_fragment_shader != VK_NULL_HANDLE;
}

void Renderer::DestroyShaders()
{
  auto DestroyShader = [this](VkShaderModule& shader) {
    if (shader != VK_NULL_HANDLE)
    {
      vkDestroyShaderModule(g_vulkan_context->GetDevice(), shader, nullptr);
      shader = VK_NULL_HANDLE;
    }
  };

  DestroyShader(m_clear_fragment_shader);
}

}  // namespace Vulkan
