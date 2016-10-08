// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/Renderer.h"

#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>

#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/Vulkan/BoundingBox.h"
#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/RasterFont.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
Renderer::Renderer(std::unique_ptr<SwapChain> swap_chain) : m_swap_chain(std::move(swap_chain))
{
  g_Config.bRunning = true;
  UpdateActiveConfig();

  // Set to something invalid, forcing all states to be re-initialized.
  for (size_t i = 0; i < m_sampler_states.size(); i++)
    m_sampler_states[i].bits = std::numeric_limits<decltype(m_sampler_states[i].bits)>::max();

  // These have to be initialized before FramebufferManager is created.
  // If running surfaceless, assume a window size of MAX_XFB_{WIDTH,HEIGHT}.
  FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
  FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);
  s_backbuffer_width = m_swap_chain ? m_swap_chain->GetWidth() : MAX_XFB_WIDTH;
  s_backbuffer_height = m_swap_chain ? m_swap_chain->GetHeight() : MAX_XFB_HEIGHT;
  s_last_efb_scale = g_ActiveConfig.iEFBScale;
  UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
  CalculateTargetSize(s_backbuffer_width, s_backbuffer_height);
  PixelShaderManager::SetEfbScaleChanged();
}

Renderer::~Renderer()
{
  g_Config.bRunning = false;
  UpdateActiveConfig();
  DestroyScreenshotResources();
  DestroyShaders();
  DestroySemaphores();
}

bool Renderer::Initialize(FramebufferManager* framebuffer_mgr)
{
  m_framebuffer_mgr = framebuffer_mgr;
  m_state_tracker = std::make_unique<StateTracker>();
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
    m_state_tracker->SetBBoxBuffer(m_bounding_box->GetGPUBuffer(),
                                   m_bounding_box->GetGPUBufferOffset(),
                                   m_bounding_box->GetGPUBufferSize());
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
  if (type == PEEK_COLOR)
  {
    u32 color = m_framebuffer_mgr->PeekEFBColor(m_state_tracker.get(), x, y);

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
  else  // if (type == PEEK_Z)
  {
    // Depth buffer is inverted for improved precision near far plane
    float depth = 1.0f - m_framebuffer_mgr->PeekEFBDepth(m_state_tracker.get(), x, y);
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
  if (type == POKE_COLOR)
  {
    for (size_t i = 0; i < num_points; i++)
    {
      // Convert to expected format (BGRA->RGBA)
      // TODO: Check alpha, depending on mode?
      const EfbPokeData& point = points[i];
      u32 color = ((point.data & 0xFF00FF00) | ((point.data >> 16) & 0xFF) |
                   ((point.data << 16) & 0xFF0000));
      m_framebuffer_mgr->PokeEFBColor(m_state_tracker.get(), point.x, point.y, color);
    }
  }
  else  // if (type == POKE_Z)
  {
    for (size_t i = 0; i < num_points; i++)
    {
      // Convert to floating-point depth.
      const EfbPokeData& point = points[i];
      float depth = (1.0f - float(point.data & 0xFFFFFF) / 16777216.0f);
      m_framebuffer_mgr->PokeEFBDepth(m_state_tracker.get(), point.x, point.y, depth);
    }
  }
}

u16 Renderer::BBoxRead(int index)
{
  s32 value = m_bounding_box->Get(m_state_tracker.get(), static_cast<size_t>(index));

  // Here we get the min/max value of the truncated position of the upscaled framebuffer.
  // So we have to correct them to the unscaled EFB sizes.
  if (index < 2)
  {
    // left/right
    value = value * EFB_WIDTH / s_target_width;
  }
  else
  {
    // up/down
    value = value * EFB_HEIGHT / s_target_height;
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
    scaled_value = scaled_value * s_target_width / EFB_WIDTH;
  }
  else
  {
    // up/down
    scaled_value = scaled_value * s_target_height / EFB_HEIGHT;
  }

  m_bounding_box->Set(m_state_tracker.get(), static_cast<size_t>(index), scaled_value);
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
  m_state_tracker->InvalidateDescriptorSets();
  m_state_tracker->SetPendingRebind();
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool color_enable, bool alpha_enable,
                           bool z_enable, u32 color, u32 z)
{
  // Native -> EFB coordinates
  TargetRectangle target_rc = Renderer::ConvertEFBRectangle(rc);
  VkRect2D target_vk_rc = {
      {target_rc.left, target_rc.top},
      {static_cast<uint32_t>(target_rc.GetWidth()), static_cast<uint32_t>(target_rc.GetHeight())}};

  // Convert RGBA8 -> floating-point values.
  VkClearValue clear_color_value = {};
  VkClearValue clear_depth_value = {};
  clear_color_value.color.float32[0] = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
  clear_color_value.color.float32[1] = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
  clear_color_value.color.float32[2] = static_cast<float>((color >> 0) & 0xFF) / 255.0f;
  clear_color_value.color.float32[3] = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
  clear_depth_value.depthStencil.depth = (1.0f - (static_cast<float>(z & 0xFFFFFF) / 16777216.0f));

  // Determine whether the EFB has an alpha channel. If it doesn't, we can clear the alpha
  // channel to 0xFF. This hopefully allows us to use the fast path in most cases.
  if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16 ||
      bpmem.zcontrol.pixel_format == PEControl::RGB8_Z24 ||
      bpmem.zcontrol.pixel_format == PEControl::Z24)
  {
    // Force alpha writes, and set the color to 0xFF.
    alpha_enable = true;
    color |= 0xFF000000;
  }

  // If we're not in a render pass (start of the frame), we can use a clear render pass
  // to discard the data, rather than loading and then clearing.
  bool use_clear_render_pass = (color_enable && alpha_enable && z_enable);
  if (m_state_tracker->InRenderPass())
  {
    // Prefer not to end a render pass just to do a clear.
    use_clear_render_pass = false;
  }

  // Fastest path: Use a render pass to clear the buffers.
  if (use_clear_render_pass)
  {
    VkClearValue clear_values[2] = {clear_color_value, clear_depth_value};
    m_state_tracker->BeginClearRenderPass(target_vk_rc, clear_values);
    return;
  }

  // Fast path: Use vkCmdClearAttachments to clear the buffers within a render path
  // We can't use this when preserving alpha but clearing color.
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
      VkClearRect clear_rect = {target_vk_rc, 0, m_framebuffer_mgr->GetEFBLayers()};
      if (!m_state_tracker->IsWithinRenderArea(target_vk_rc.offset.x, target_vk_rc.offset.y,
                                               target_vk_rc.extent.width,
                                               target_vk_rc.extent.height))
      {
        m_state_tracker->EndClearRenderPass();
      }
      m_state_tracker->BeginRenderPass();

      vkCmdClearAttachments(g_command_buffer_mgr->GetCurrentCommandBuffer(), num_clear_attachments,
                            clear_attachments, 1, &clear_rect);
    }
  }

  // Anything left over for the slow path?
  if (!color_enable && !alpha_enable && !z_enable)
    return;

  // Clearing must occur within a render pass.
  if (!m_state_tracker->IsWithinRenderArea(target_vk_rc.offset.x, target_vk_rc.offset.y,
                                           target_vk_rc.extent.width, target_vk_rc.extent.height))
  {
    m_state_tracker->EndClearRenderPass();
  }
  m_state_tracker->BeginRenderPass();
  m_state_tracker->SetPendingRebind();

  // Mask away the appropriate colors and use a shader
  BlendState blend_state = Util::GetNoBlendingBlendState();
  u32 write_mask = 0;
  if (color_enable)
    write_mask |= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
  if (alpha_enable)
    write_mask |= VK_COLOR_COMPONENT_A_BIT;
  blend_state.write_mask = write_mask;

  DepthStencilState depth_state = Util::GetNoDepthTestingDepthStencilState();
  depth_state.test_enable = z_enable ? VK_TRUE : VK_FALSE;
  depth_state.write_enable = z_enable ? VK_TRUE : VK_FALSE;
  depth_state.compare_op = VK_COMPARE_OP_ALWAYS;

  RasterizationState rs_state = Util::GetNoCullRasterizationState();
  rs_state.per_sample_shading = g_ActiveConfig.bSSAA ? VK_TRUE : VK_FALSE;
  rs_state.samples = m_framebuffer_mgr->GetEFBSamples();

  // No need to start a new render pass, but we do need to restore viewport state
  UtilityShaderDraw draw(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), g_object_cache->GetStandardPipelineLayout(),
      m_framebuffer_mgr->GetEFBLoadRenderPass(), g_object_cache->GetPassthroughVertexShader(),
      g_object_cache->GetPassthroughGeometryShader(), m_clear_fragment_shader);

  draw.SetRasterizationState(rs_state);
  draw.SetDepthStencilState(depth_state);
  draw.SetBlendState(blend_state);

  draw.DrawColoredQuad(target_rc.left, target_rc.top, target_rc.GetWidth(), target_rc.GetHeight(),
                       clear_color_value.color.float32[0], clear_color_value.color.float32[1],
                       clear_color_value.color.float32[2], clear_color_value.color.float32[3],
                       clear_depth_value.depthStencil.depth);
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
  m_state_tracker->EndRenderPass();
  m_state_tracker->SetPendingRebind();
  m_framebuffer_mgr->ReinterpretPixelData(convtype);

  // EFB framebuffer has now changed, so update accordingly.
  BindEFBToStateTracker();
}

void Renderer::SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height,
                        const EFBRectangle& rc, float gamma)
{
  // Flush any pending EFB pokes.
  m_framebuffer_mgr->FlushEFBPokes(m_state_tracker.get());

  // End the current render pass.
  m_state_tracker->EndRenderPass();
  m_state_tracker->OnEndFrame();

  // Scale the source rectangle to the selected internal resolution.
  TargetRectangle source_rc = Renderer::ConvertEFBRectangle(rc);

  // Transition the EFB render target to a shader resource.
  VkRect2D src_region = {{0, 0},
                         {m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight()}};
  Texture2D* efb_color_texture =
      m_framebuffer_mgr->ResolveEFBColorTexture(m_state_tracker.get(), src_region);
  efb_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Draw to the screenshot buffer if needed.
  if (IsFrameDumping() && DrawScreenshot(source_rc, efb_color_texture))
  {
    DumpFrameData(reinterpret_cast<const u8*>(m_screenshot_readback_texture->GetMapPointer()),
                  static_cast<int>(m_screenshot_render_texture->GetWidth()),
                  static_cast<int>(m_screenshot_render_texture->GetHeight()),
                  static_cast<int>(m_screenshot_readback_texture->GetRowStride()));
    FinishFrameData();
  }

  // Restore the EFB color texture to color attachment ready for rendering the next frame.
  m_framebuffer_mgr->GetEFBColorTexture()->TransitionToLayout(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Ensure the worker thread is not still submitting a previous command buffer.
  // In other words, the last frame has been submitted (otherwise the next call would
  // be a race, as the image may not have been consumed yet).
  g_command_buffer_mgr->PrepareToSubmitCommandBuffer();

  // Draw to the screen if we have a swap chain.
  if (m_swap_chain)
  {
    DrawScreen(source_rc, efb_color_texture);

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

  // Determine what (if anything) has changed in the config.
  CheckForConfigChanges();

  // Handle host window resizes.
  CheckForSurfaceChange();

  // Handle output size changes from the guest.
  // There is a downside to doing this here is that if the game changes its XFB source area,
  // the changes will be delayed by one frame. For the moment it has to be done here because
  // this can cause a target size change, which would result in a black frame if done earlier.
  CheckForTargetResize(fb_width, fb_stride, fb_height);

  // Clean up stale textures.
  TextureCacheBase::Cleanup(frameCount);
}

void Renderer::DrawScreen(const TargetRectangle& src_rect, const Texture2D* src_tex)
{
  // Grab the next image from the swap chain in preparation for drawing the window.
  VkResult res = m_swap_chain->AcquireNextImage(m_image_available_semaphore);
  if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
  {
    // Window has been resized. Update the swap chain and try again.
    ResizeSwapChain();
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

  // Blit the EFB to the back buffer (Swap chain)
  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetStandardPipelineLayout(), m_swap_chain->GetRenderPass(),
                         g_object_cache->GetPassthroughVertexShader(), VK_NULL_HANDLE,
                         m_blit_fragment_shader);

  // Begin the present render pass
  VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  VkRect2D target_region = {{0, 0}, {backbuffer->GetWidth(), backbuffer->GetHeight()}};
  draw.BeginRenderPass(m_swap_chain->GetCurrentFramebuffer(), target_region, &clear_value);

  // Copy EFB -> backbuffer
  const TargetRectangle& dst_rect = GetTargetRectangle();
  BlitScreen(m_swap_chain->GetRenderPass(), dst_rect, src_rect, src_tex, true);

  // OSD stuff
  Util::SetViewportAndScissor(g_command_buffer_mgr->GetCurrentCommandBuffer(), 0, 0,
                              backbuffer->GetWidth(), backbuffer->GetHeight());
  DrawDebugText();

  // Do our OSD callbacks
  OSD::DoCallbacks(OSD::CallbackType::OnFrame);
  OSD::DrawMessages();

  // End drawing to backbuffer
  draw.EndRenderPass();

  // Transition the backbuffer to PRESENT_SRC to ensure all commands drawing
  // to it have finished before present.
  backbuffer->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

bool Renderer::DrawScreenshot(const TargetRectangle& src_rect, const Texture2D* src_tex)
{
  // Draw the screenshot to an image containing only the active screen area, removing any
  // borders as a result of the game rendering in a different aspect ratio.
  TargetRectangle target_rect = GetTargetRectangle();
  target_rect.right = target_rect.GetWidth();
  target_rect.bottom = target_rect.GetHeight();
  target_rect.left = 0;
  target_rect.top = 0;
  u32 width = std::max(1u, static_cast<u32>(target_rect.GetWidth()));
  u32 height = std::max(1u, static_cast<u32>(target_rect.GetHeight()));
  if (!ResizeScreenshotBuffer(width, height))
    return false;

  VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  VkClearRect clear_rect = {{{0, 0}, {width, height}}, 0, 1};
  VkClearAttachment clear_attachment = {VK_IMAGE_ASPECT_COLOR_BIT, 0, clear_value};
  VkRenderPassBeginInfo info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                nullptr,
                                m_framebuffer_mgr->GetColorCopyForReadbackRenderPass(),
                                m_screenshot_framebuffer,
                                {{0, 0}, {width, height}},
                                1,
                                &clear_value};
  vkCmdBeginRenderPass(g_command_buffer_mgr->GetCurrentCommandBuffer(), &info,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdClearAttachments(g_command_buffer_mgr->GetCurrentCommandBuffer(), 1, &clear_attachment, 1,
                        &clear_rect);
  BlitScreen(m_framebuffer_mgr->GetColorCopyForReadbackRenderPass(), target_rect, src_rect, src_tex,
             true);
  vkCmdEndRenderPass(g_command_buffer_mgr->GetCurrentCommandBuffer());

  // Copy to the readback texture.
  m_screenshot_readback_texture->CopyFromImage(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), m_screenshot_render_texture->GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, width, height, 0, 0);

  // Wait for the command buffer to complete.
  g_command_buffer_mgr->ExecuteCommandBuffer(false, true);
  return true;
}

void Renderer::BlitScreen(VkRenderPass render_pass, const TargetRectangle& dst_rect,
                          const TargetRectangle& src_rect, const Texture2D* src_tex,
                          bool linear_filter)
{
  // We could potentially use vkCmdBlitImage here.
  VkSampler sampler =
      linear_filter ? g_object_cache->GetLinearSampler() : g_object_cache->GetPointSampler();

  // Set up common data
  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetStandardPipelineLayout(), render_pass,
                         g_object_cache->GetPassthroughVertexShader(), VK_NULL_HANDLE,
                         m_blit_fragment_shader);

  draw.SetPSSampler(0, src_tex->GetView(), sampler);

  if (g_ActiveConfig.iStereoMode == STEREO_SBS || g_ActiveConfig.iStereoMode == STEREO_TAB)
  {
    TargetRectangle left_rect;
    TargetRectangle right_rect;
    if (g_ActiveConfig.iStereoMode == STEREO_TAB)
      ConvertStereoRectangle(dst_rect, right_rect, left_rect);
    else
      ConvertStereoRectangle(dst_rect, left_rect, right_rect);

    draw.DrawQuad(left_rect.left, left_rect.top, left_rect.GetWidth(), left_rect.GetHeight(),
                  src_rect.left, src_rect.top, 0, src_rect.GetWidth(), src_rect.GetHeight(),
                  src_tex->GetWidth(), src_tex->GetHeight());

    draw.DrawQuad(right_rect.left, right_rect.top, right_rect.GetWidth(), right_rect.GetHeight(),
                  src_rect.left, src_rect.top, 1, src_rect.GetWidth(), src_rect.GetHeight(),
                  src_tex->GetWidth(), src_tex->GetHeight());
  }
  else
  {
    draw.DrawQuad(dst_rect.left, dst_rect.top, dst_rect.GetWidth(), dst_rect.GetHeight(),
                  src_rect.left, src_rect.top, 0, src_rect.GetWidth(), src_rect.GetHeight(),
                  src_tex->GetWidth(), src_tex->GetHeight());
  }
}

bool Renderer::ResizeScreenshotBuffer(u32 new_width, u32 new_height)
{
  if (m_screenshot_render_texture && m_screenshot_render_texture->GetWidth() == new_width &&
      m_screenshot_render_texture->GetHeight() == new_height)
  {
    return true;
  }

  if (m_screenshot_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_screenshot_framebuffer, nullptr);
    m_screenshot_framebuffer = VK_NULL_HANDLE;
  }

  m_screenshot_render_texture =
      Texture2D::Create(new_width, new_height, 1, 1, EFB_COLOR_TEXTURE_FORMAT,
                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

  m_screenshot_readback_texture = StagingTexture2D::Create(STAGING_BUFFER_TYPE_READBACK, new_width,
                                                           new_height, EFB_COLOR_TEXTURE_FORMAT);
  if (!m_screenshot_render_texture || !m_screenshot_readback_texture ||
      !m_screenshot_readback_texture->Map())
  {
    WARN_LOG(VIDEO, "Failed to resize screenshot render texture");
    m_screenshot_render_texture.reset();
    m_screenshot_readback_texture.reset();
    return false;
  }

  VkImageView attachment = m_screenshot_render_texture->GetView();
  VkFramebufferCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  info.renderPass = m_framebuffer_mgr->GetColorCopyForReadbackRenderPass();
  info.attachmentCount = 1;
  info.pAttachments = &attachment;
  info.width = new_width;
  info.height = new_height;
  info.layers = 1;

  VkResult res =
      vkCreateFramebuffer(g_vulkan_context->GetDevice(), &info, nullptr, &m_screenshot_framebuffer);
  if (res != VK_SUCCESS)
  {
    WARN_LOG(VIDEO, "Failed to resize screenshot framebuffer");
    m_screenshot_render_texture.reset();
    m_screenshot_readback_texture.reset();
    return false;
  }

  // Render pass expects texture is in transfer src to start with.
  m_screenshot_render_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  return true;
}

void Renderer::DestroyScreenshotResources()
{
  if (m_screenshot_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_screenshot_framebuffer, nullptr);
    m_screenshot_framebuffer = VK_NULL_HANDLE;
  }

  m_screenshot_render_texture.reset();
  m_screenshot_readback_texture.reset();
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

  // Changing the XFB source area will likely change the final drawing rectangle.
  UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
  if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height))
  {
    PixelShaderManager::SetEfbScaleChanged();
    ResizeEFBTextures();
  }

  // This call is needed for auto-resizing to work.
  SetWindowSize(static_cast<int>(fb_stride), static_cast<int>(fb_height));
}

void Renderer::CheckForSurfaceChange()
{
  if (!s_surface_needs_change.IsSet())
    return;

  u32 old_width = m_swap_chain ? m_swap_chain->GetWidth() : 0;
  u32 old_height = m_swap_chain ? m_swap_chain->GetHeight() : 0;

  // Fast path, if the surface handle is the same, the window has just been resized.
  if (m_swap_chain && s_new_surface_handle == m_swap_chain->GetNativeHandle())
  {
    INFO_LOG(VIDEO, "Detected window resize.");
    ResizeSwapChain();

    // Notify the main thread we are done.
    s_surface_needs_change.Clear();
    s_new_surface_handle = nullptr;
    s_surface_changed.Set();
  }
  else
  {
    // Wait for the GPU to catch up since we're going to destroy the swap chain.
    g_command_buffer_mgr->WaitForGPUIdle();

    // Did we previously have a swap chain?
    if (m_swap_chain)
    {
      if (!s_new_surface_handle)
      {
        // If there is no surface now, destroy the swap chain.
        m_swap_chain.reset();
      }
      else
      {
        // Recreate the surface. If this fails we're in trouble.
        if (!m_swap_chain->RecreateSurface(s_new_surface_handle))
          PanicAlert("Failed to recreate Vulkan surface. Cannot continue.");
      }
    }
    else
    {
      // Previously had no swap chain. So create one.
      VkSurfaceKHR surface = SwapChain::CreateVulkanSurface(g_vulkan_context->GetVulkanInstance(),
                                                            s_new_surface_handle);
      if (surface != VK_NULL_HANDLE)
      {
        m_swap_chain = SwapChain::Create(s_new_surface_handle, surface, g_ActiveConfig.IsVSync());
        if (!m_swap_chain)
          PanicAlert("Failed to create swap chain.");
      }
      else
      {
        PanicAlert("Failed to create surface.");
      }
    }

    // Notify calling thread.
    s_surface_needs_change.Clear();
    s_new_surface_handle = nullptr;
    s_surface_changed.Set();
  }

  if (m_swap_chain)
  {
    // Handle case where the dimensions are now different
    if (old_width != m_swap_chain->GetWidth() || old_height != m_swap_chain->GetHeight())
      OnSwapChainResized();
  }
}

void Renderer::CheckForConfigChanges()
{
  // Save the video config so we can compare against to determine which settings have changed.
  int old_multisamples = g_ActiveConfig.iMultisamples;
  int old_anisotropy = g_ActiveConfig.iMaxAnisotropy;
  int old_stereo_mode = g_ActiveConfig.iStereoMode;
  int old_aspect_ratio = g_ActiveConfig.iAspectRatio;
  bool old_force_filtering = g_ActiveConfig.bForceFiltering;
  bool old_ssaa = g_ActiveConfig.bSSAA;

  // Copy g_Config to g_ActiveConfig.
  // NOTE: This can potentially race with the UI thread, however if it does, the changes will be
  // delayed until the next time CheckForConfigChanges is called.
  UpdateActiveConfig();

  // Determine which (if any) settings have changed.
  bool msaa_changed = old_multisamples != g_ActiveConfig.iMultisamples;
  bool ssaa_changed = old_ssaa != g_ActiveConfig.bSSAA;
  bool anisotropy_changed = old_anisotropy != g_ActiveConfig.iMaxAnisotropy;
  bool force_texture_filtering_changed = old_force_filtering != g_ActiveConfig.bForceFiltering;
  bool stereo_changed = old_stereo_mode != g_ActiveConfig.iStereoMode;
  bool efb_scale_changed = s_last_efb_scale != g_ActiveConfig.iEFBScale;
  bool aspect_changed = old_aspect_ratio != g_ActiveConfig.iAspectRatio;

  // Update texture cache settings with any changed options.
  TextureCache::OnConfigChanged(g_ActiveConfig);

  // Handle internal resolution changes.
  if (efb_scale_changed)
    s_last_efb_scale = g_ActiveConfig.iEFBScale;

  // If the aspect ratio is changed, this changes the area that the game is drawn to.
  if (aspect_changed)
    UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

  if (efb_scale_changed || aspect_changed)
  {
    if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height))
      ResizeEFBTextures();
  }

  // MSAA samples changed, we need to recreate the EFB render pass.
  // If the stereoscopy mode changed, we need to recreate the buffers as well.
  if (msaa_changed || stereo_changed)
  {
    g_command_buffer_mgr->WaitForGPUIdle();
    m_framebuffer_mgr->RecreateRenderPass();
    m_framebuffer_mgr->ResizeEFBTextures();
    BindEFBToStateTracker();
  }

  // SSAA changed on/off, we can leave the buffers/render pass, but have to recompile shaders.
  // Changing stereoscopy from off<->on also requires shaders to be recompiled.
  if (msaa_changed || ssaa_changed || stereo_changed)
  {
    g_command_buffer_mgr->WaitForGPUIdle();
    RecompileShaders();
    m_framebuffer_mgr->RecompileShaders();
    g_object_cache->ClearPipelineCache();
  }

  // For vsync, we need to change the present mode, which means recreating the swap chain.
  if (m_swap_chain && g_ActiveConfig.IsVSync() != m_swap_chain->IsVSyncEnabled())
  {
    g_command_buffer_mgr->WaitForGPUIdle();
    m_swap_chain->SetVSync(g_ActiveConfig.IsVSync());
  }

  // Wipe sampler cache if force texture filtering or anisotropy changes.
  if (anisotropy_changed || force_texture_filtering_changed)
    ResetSamplerStates();
}

void Renderer::OnSwapChainResized()
{
  s_backbuffer_width = m_swap_chain->GetWidth();
  s_backbuffer_height = m_swap_chain->GetHeight();
  UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
  if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height))
  {
    PixelShaderManager::SetEfbScaleChanged();
    ResizeEFBTextures();
  }
}

void Renderer::BindEFBToStateTracker()
{
  // Update framebuffer in state tracker
  VkRect2D framebuffer_size = {
      {0, 0}, {m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight()}};
  m_state_tracker->SetRenderPass(m_framebuffer_mgr->GetEFBLoadRenderPass(),
                                 m_framebuffer_mgr->GetEFBClearRenderPass());
  m_state_tracker->SetFramebuffer(m_framebuffer_mgr->GetEFBFramebuffer(), framebuffer_size);

  // Update rasterization state with MSAA info
  RasterizationState rs_state = {};
  rs_state.bits = m_state_tracker->GetRasterizationState().bits;
  rs_state.samples = m_framebuffer_mgr->GetEFBSamples();
  rs_state.per_sample_shading = g_ActiveConfig.bSSAA ? VK_TRUE : VK_FALSE;
  m_state_tracker->SetRasterizationState(rs_state);
}

void Renderer::ResizeEFBTextures()
{
  // Ensure the GPU is finished with the current EFB textures.
  g_command_buffer_mgr->WaitForGPUIdle();
  m_framebuffer_mgr->ResizeEFBTextures();
  BindEFBToStateTracker();

  // Viewport and scissor rect have to be reset since they will be scaled differently.
  SetViewport();
  BPFunctions::SetScissor();
}

void Renderer::ResizeSwapChain()
{
  // The worker thread may still be submitting a present on this swap chain.
  g_command_buffer_mgr->WaitForGPUIdle();

  // It's now safe to resize the swap chain.
  if (!m_swap_chain->ResizeSwapChain())
    PanicAlert("Failed to resize swap chain.");

  OnSwapChainResized();
}

void Renderer::ApplyState(bool bUseDstAlpha)
{
}

void Renderer::ResetAPIState()
{
  // End the EFB render pass if active
  m_state_tracker->EndRenderPass();
}

void Renderer::RestoreAPIState()
{
  // Instruct the state tracker to re-bind everything before the next draw
  m_state_tracker->SetPendingRebind();
}

void Renderer::SetGenerationMode()
{
  RasterizationState new_rs_state = {};
  new_rs_state.bits = m_state_tracker->GetRasterizationState().bits;

  switch (bpmem.genMode.cullmode)
  {
  case GenMode::CULL_NONE:
    new_rs_state.cull_mode = VK_CULL_MODE_NONE;
    break;
  case GenMode::CULL_BACK:
    new_rs_state.cull_mode = VK_CULL_MODE_BACK_BIT;
    break;
  case GenMode::CULL_FRONT:
    new_rs_state.cull_mode = VK_CULL_MODE_FRONT_BIT;
    break;
  case GenMode::CULL_ALL:
    new_rs_state.cull_mode = VK_CULL_MODE_FRONT_AND_BACK;
    break;
  default:
    new_rs_state.cull_mode = VK_CULL_MODE_NONE;
    break;
  }

  m_state_tracker->SetRasterizationState(new_rs_state);
}

void Renderer::SetDepthMode()
{
  DepthStencilState new_ds_state = {};
  new_ds_state.test_enable = bpmem.zmode.testenable ? VK_TRUE : VK_FALSE;
  new_ds_state.write_enable = bpmem.zmode.updateenable ? VK_TRUE : VK_FALSE;

  // Inverted depth, hence these are swapped
  switch (bpmem.zmode.func)
  {
  case ZMode::NEVER:
    new_ds_state.compare_op = VK_COMPARE_OP_NEVER;
    break;
  case ZMode::LESS:
    new_ds_state.compare_op = VK_COMPARE_OP_GREATER;
    break;
  case ZMode::EQUAL:
    new_ds_state.compare_op = VK_COMPARE_OP_EQUAL;
    break;
  case ZMode::LEQUAL:
    new_ds_state.compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL;
    break;
  case ZMode::GREATER:
    new_ds_state.compare_op = VK_COMPARE_OP_LESS;
    break;
  case ZMode::NEQUAL:
    new_ds_state.compare_op = VK_COMPARE_OP_NOT_EQUAL;
    break;
  case ZMode::GEQUAL:
    new_ds_state.compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    break;
  case ZMode::ALWAYS:
    new_ds_state.compare_op = VK_COMPARE_OP_ALWAYS;
    break;
  default:
    new_ds_state.compare_op = VK_COMPARE_OP_ALWAYS;
    break;
  }

  m_state_tracker->SetDepthStencilState(new_ds_state);
}

void Renderer::SetColorMask()
{
  u32 color_mask = 0;

  if (bpmem.alpha_test.TestResult() != AlphaTest::FAIL)
  {
    if (bpmem.blendmode.alphaupdate && bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24)
      color_mask |= VK_COLOR_COMPONENT_A_BIT;
    if (bpmem.blendmode.colorupdate)
      color_mask |= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
  }

  BlendState new_blend_state = {};
  new_blend_state.bits = m_state_tracker->GetBlendState().bits;
  new_blend_state.write_mask = color_mask;

  m_state_tracker->SetBlendState(new_blend_state);
}

void Renderer::SetBlendMode(bool force_update)
{
  BlendState new_blend_state = {};
  new_blend_state.bits = m_state_tracker->GetBlendState().bits;

  // Fast path for blending disabled
  if (!bpmem.blendmode.blendenable)
  {
    new_blend_state.blend_enable = VK_FALSE;
    new_blend_state.blend_op = VK_BLEND_OP_ADD;
    new_blend_state.src_blend = VK_BLEND_FACTOR_ONE;
    new_blend_state.dst_blend = VK_BLEND_FACTOR_ZERO;
    new_blend_state.alpha_blend_op = VK_BLEND_OP_ADD;
    new_blend_state.src_alpha_blend = VK_BLEND_FACTOR_ONE;
    new_blend_state.dst_alpha_blend = VK_BLEND_FACTOR_ZERO;
    m_state_tracker->SetBlendState(new_blend_state);
    return;
  }
  // Fast path for subtract blending
  else if (bpmem.blendmode.subtract)
  {
    new_blend_state.blend_enable = VK_TRUE;
    new_blend_state.blend_op = VK_BLEND_OP_REVERSE_SUBTRACT;
    new_blend_state.src_blend = VK_BLEND_FACTOR_ONE;
    new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE;
    new_blend_state.alpha_blend_op = VK_BLEND_OP_REVERSE_SUBTRACT;
    new_blend_state.src_alpha_blend = VK_BLEND_FACTOR_ONE;
    new_blend_state.dst_alpha_blend = VK_BLEND_FACTOR_ONE;
    m_state_tracker->SetBlendState(new_blend_state);
    return;
  }

  // Our render target always uses an alpha channel, so we need to override the blend functions to
  // assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel.
  bool target_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
  bool use_dst_alpha = bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate && target_has_alpha &&
                       g_vulkan_context->SupportsDualSourceBlend();

  new_blend_state.blend_enable = VK_TRUE;
  new_blend_state.blend_op = VK_BLEND_OP_ADD;

  switch (bpmem.blendmode.srcfactor)
  {
  case BlendMode::ZERO:
    new_blend_state.src_blend = VK_BLEND_FACTOR_ZERO;
    break;
  case BlendMode::ONE:
    new_blend_state.src_blend = VK_BLEND_FACTOR_ONE;
    break;
  case BlendMode::DSTCLR:
    new_blend_state.src_blend = VK_BLEND_FACTOR_DST_COLOR;
    break;
  case BlendMode::INVDSTCLR:
    new_blend_state.src_blend = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    break;
  case BlendMode::SRCALPHA:
    new_blend_state.src_blend =
        use_dst_alpha ? VK_BLEND_FACTOR_SRC1_ALPHA : VK_BLEND_FACTOR_SRC_ALPHA;
    break;
  case BlendMode::INVSRCALPHA:
    new_blend_state.src_blend =
        use_dst_alpha ? VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    break;
  case BlendMode::DSTALPHA:
    new_blend_state.src_blend = target_has_alpha ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_ONE;
    break;
  case BlendMode::INVDSTALPHA:
    new_blend_state.src_blend =
        target_has_alpha ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ZERO;
    break;
  default:
    new_blend_state.src_blend = VK_BLEND_FACTOR_ONE;
    break;
  }

  switch (bpmem.blendmode.dstfactor)
  {
  case BlendMode::ZERO:
    new_blend_state.dst_blend = VK_BLEND_FACTOR_ZERO;
    break;
  case BlendMode::ONE:
    new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE;
    break;
  case BlendMode::SRCCLR:
    new_blend_state.dst_blend = VK_BLEND_FACTOR_SRC_COLOR;
    break;
  case BlendMode::INVSRCCLR:
    new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    break;
  case BlendMode::SRCALPHA:
    new_blend_state.dst_blend =
        use_dst_alpha ? VK_BLEND_FACTOR_SRC1_ALPHA : VK_BLEND_FACTOR_SRC_ALPHA;
    break;
  case BlendMode::INVSRCALPHA:
    new_blend_state.dst_blend =
        use_dst_alpha ? VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    break;
  case BlendMode::DSTALPHA:
    new_blend_state.dst_blend = target_has_alpha ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_ONE;
    break;
  case BlendMode::INVDSTALPHA:
    new_blend_state.dst_blend =
        target_has_alpha ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ZERO;
    break;
  default:
    new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE;
    break;
  }

  if (use_dst_alpha)
  {
    // Destination alpha sets 1*SRC
    new_blend_state.alpha_blend_op = VK_BLEND_OP_ADD;
    new_blend_state.src_alpha_blend = VK_BLEND_FACTOR_ONE;
    new_blend_state.dst_alpha_blend = VK_BLEND_FACTOR_ZERO;
  }
  else
  {
    new_blend_state.alpha_blend_op = VK_BLEND_OP_ADD;
    new_blend_state.src_alpha_blend = Util::GetAlphaBlendFactor(new_blend_state.src_blend);
    new_blend_state.dst_alpha_blend = Util::GetAlphaBlendFactor(new_blend_state.dst_blend);
  }

  m_state_tracker->SetBlendState(new_blend_state);
}

void Renderer::SetLogicOpMode()
{
  BlendState new_blend_state = {};
  new_blend_state.bits = m_state_tracker->GetBlendState().bits;

  // Does our device support logic ops?
  bool logic_op_enable = bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable;
  if (g_vulkan_context->SupportsLogicOps())
  {
    if (logic_op_enable)
    {
      static const std::array<VkLogicOp, 16> logic_ops = {
          {VK_LOGIC_OP_CLEAR, VK_LOGIC_OP_AND, VK_LOGIC_OP_AND_REVERSE, VK_LOGIC_OP_COPY,
           VK_LOGIC_OP_AND_INVERTED, VK_LOGIC_OP_NO_OP, VK_LOGIC_OP_XOR, VK_LOGIC_OP_OR,
           VK_LOGIC_OP_NOR, VK_LOGIC_OP_EQUIVALENT, VK_LOGIC_OP_INVERT, VK_LOGIC_OP_OR_REVERSE,
           VK_LOGIC_OP_COPY_INVERTED, VK_LOGIC_OP_OR_INVERTED, VK_LOGIC_OP_NAND, VK_LOGIC_OP_SET}};

      new_blend_state.logic_op_enable = VK_TRUE;
      new_blend_state.logic_op = logic_ops[bpmem.blendmode.logicmode];
    }
    else
    {
      new_blend_state.logic_op_enable = VK_FALSE;
      new_blend_state.logic_op = VK_LOGIC_OP_CLEAR;
    }

    m_state_tracker->SetBlendState(new_blend_state);
  }
  else
  {
    // No logic op support, approximate with blending instead.
    // This is by no means correct, but necessary for some devices.
    if (logic_op_enable)
    {
      struct LogicOpBlend
      {
        VkBlendFactor src_factor;
        VkBlendOp op;
        VkBlendFactor dst_factor;
      };
      static const std::array<LogicOpBlend, 16> logic_ops = {
          {{VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO},
           {VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO},
           {VK_BLEND_FACTOR_ONE, VK_BLEND_OP_SUBTRACT, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR},
           {VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO},
           {VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_FACTOR_ONE},
           {VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE},
           {VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_OP_MAX,
            VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR},
           {VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE},
           {VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_OP_MAX,
            VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR},
           {VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_OP_MAX, VK_BLEND_FACTOR_SRC_COLOR},
           {VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR},
           {VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR},
           {VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR},
           {VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE},
           {VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR},
           {VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE}}};

      new_blend_state.blend_enable = VK_TRUE;
      new_blend_state.blend_op = logic_ops[bpmem.blendmode.logicmode].op;
      new_blend_state.src_blend = logic_ops[bpmem.blendmode.logicmode].src_factor;
      new_blend_state.dst_blend = logic_ops[bpmem.blendmode.logicmode].dst_factor;
      new_blend_state.alpha_blend_op = new_blend_state.blend_op;
      new_blend_state.src_alpha_blend = Util::GetAlphaBlendFactor(new_blend_state.src_blend);
      new_blend_state.dst_alpha_blend = Util::GetAlphaBlendFactor(new_blend_state.dst_blend);

      m_state_tracker->SetBlendState(new_blend_state);
    }
    else
    {
      // This is unfortunate. Since we clobber the blend state when enabling logic ops,
      // we have to call SetBlendMode again to restore the current blend state.
      SetBlendMode(true);
      return;
    }
  }
}

void Renderer::SetSamplerState(int stage, int texindex, bool custom_tex)
{
  const FourTexUnits& tex = bpmem.tex[texindex];
  const TexMode0& tm0 = tex.texMode0[stage];
  const TexMode1& tm1 = tex.texMode1[stage];
  SamplerState new_state = {};

  if (g_ActiveConfig.bForceFiltering)
  {
    new_state.min_filter = VK_FILTER_LINEAR;
    new_state.mag_filter = VK_FILTER_LINEAR;
    new_state.mipmap_mode = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ?
                                VK_SAMPLER_MIPMAP_MODE_LINEAR :
                                VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }
  else
  {
    // Constants for these?
    new_state.min_filter = (tm0.min_filter & 4) != 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    new_state.mipmap_mode = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ?
                                VK_SAMPLER_MIPMAP_MODE_LINEAR :
                                VK_SAMPLER_MIPMAP_MODE_NEAREST;
    new_state.mag_filter = tm0.mag_filter != 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  }

  // If mipmaps are disabled, clamp min/max lod
  new_state.max_lod = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ? tm1.max_lod : 0;
  new_state.min_lod = std::min(new_state.max_lod.Value(), tm1.min_lod);
  new_state.lod_bias = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ? tm0.lod_bias : 0;

  // Custom textures may have a greater number of mips
  if (custom_tex)
    new_state.max_lod = 255;

  // Address modes
  static const VkSamplerAddressMode address_modes[] = {
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_REPEAT,
      VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT};
  new_state.wrap_u = address_modes[tm0.wrap_s];
  new_state.wrap_v = address_modes[tm0.wrap_t];

  // Only use anisotropic filtering for textures that would be linearly filtered.
  new_state.enable_anisotropic_filtering = SamplerCommon::IsBpTexMode0PointFiltering(tm0) ? 0 : 1;

  // Skip lookup if the state hasn't changed.
  size_t bind_index = (texindex * 4) + stage;
  if (m_sampler_states[bind_index].bits == new_state.bits)
    return;

  // Look up new state and replace in state tracker.
  VkSampler sampler = g_object_cache->GetSampler(new_state);
  if (sampler == VK_NULL_HANDLE)
  {
    ERROR_LOG(VIDEO, "Failed to create sampler");
    sampler = g_object_cache->GetPointSampler();
  }

  m_state_tracker->SetSampler(bind_index, sampler);
  m_sampler_states[bind_index].bits = new_state.bits;
}

void Renderer::ResetSamplerStates()
{
  // Ensure none of the sampler objects are in use.
  // This assumes that none of the samplers are in use on the command list currently being recorded.
  g_command_buffer_mgr->WaitForGPUIdle();

  // Invalidate all sampler states, next draw will re-initialize them.
  for (size_t i = 0; i < m_sampler_states.size(); i++)
  {
    m_sampler_states[i].bits = std::numeric_limits<decltype(m_sampler_states[i].bits)>::max();
    m_state_tracker->SetSampler(i, g_object_cache->GetPointSampler());
  }

  // Invalidate all sampler objects (some will be unused now).
  g_object_cache->ClearSamplerCache();
}

void Renderer::SetDitherMode()
{
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

  m_state_tracker->SetScissor(scissor);
}

void Renderer::SetViewport()
{
  int scissor_x_offset = bpmem.scissorOffset.x * 2;
  int scissor_y_offset = bpmem.scissorOffset.y * 2;

  float x = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - scissor_x_offset);
  float y = Renderer::EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - scissor_y_offset);
  float width = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
  float height = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);
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

  // If we do depth clipping and depth range in the vertex shader we only need to ensure
  // depth values don't exceed the maximum value supported by the console GPU. If not,
  // we simply clamp the near/far values themselves to the maximum value as done above.
  float min_depth, max_depth;
  if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
  {
    min_depth = 1.0f - GX_MAX_DEPTH;
    max_depth = 1.0f;
  }
  else
  {
    float near_val = MathUtil::Clamp<float>(xfmem.viewport.farZ -
                                                MathUtil::Clamp<float>(xfmem.viewport.zRange,
                                                                       -16777216.0f, 16777216.0f),
                                            0.0f, 16777215.0f) /
                     16777216.0f;
    float far_val = MathUtil::Clamp<float>(xfmem.viewport.farZ, 0.0f, 16777215.0f) / 16777216.0f;
    min_depth = 1.0f - near_val;
    max_depth = 1.0f - far_val;
  }

  VkViewport viewport = {x, y, width, height, min_depth, max_depth};
  m_state_tracker->SetViewport(viewport);
}

void Renderer::ChangeSurface(void* new_surface_handle)
{
  // Called by the main thread when the window is resized.
  s_new_surface_handle = new_surface_handle;
  s_surface_needs_change.Set();
  s_surface_changed.Set();
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

  static const char BLIT_FRAGMENT_SHADER_SOURCE[] = R"(
    layout(set = 1, binding = 0) uniform sampler2DArray samp0;

    layout(location = 0) in float3 uv0;
    layout(location = 1) in float4 col0;
    layout(location = 0) out float4 ocol0;

    void main()
    {
      ocol0 = float4(texture(samp0, uv0).xyz, 1.0);
    }
  )";

  std::string header = g_object_cache->GetUtilityShaderHeader();
  std::string source;

  source = header + CLEAR_FRAGMENT_SHADER_SOURCE;
  m_clear_fragment_shader = Util::CompileAndCreateFragmentShader(source);
  source = header + BLIT_FRAGMENT_SHADER_SOURCE;
  m_blit_fragment_shader = Util::CompileAndCreateFragmentShader(source);

  if (m_clear_fragment_shader == VK_NULL_HANDLE || m_blit_fragment_shader == VK_NULL_HANDLE)
  {
    return false;
  }

  return true;
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
  DestroyShader(m_blit_fragment_shader);
}

}  // namespace Vulkan
