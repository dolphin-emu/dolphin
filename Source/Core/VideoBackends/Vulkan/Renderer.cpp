// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <limits>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/RasterFont.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/SharedShaderCache.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/Util.h"

#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
// Init functions
Renderer::Renderer(SwapChain* swap_chain, StateTracker* state_tracker)
    : m_swap_chain(swap_chain), m_state_tracker(state_tracker)
{
  // Set to something invalid, forcing all states to be re-initialized.
  for (size_t i = 0; i < m_sampler_states.size(); i++)
    m_sampler_states[i].hex = std::numeric_limits<decltype(m_sampler_states[i].hex)>::max();
}

Renderer::~Renderer()
{
  g_Config.bRunning = false;
  UpdateActiveConfig();
  DestroySemaphores();
}

bool Renderer::Initialize()
{
  g_Config.bRunning = true;
  UpdateActiveConfig();

  // Work around the stupid static crap
  m_framebuffer_mgr = static_cast<FramebufferManager*>(g_framebuffer_manager.get());
  VkRect2D framebuffer_render_area = {
      {0, 0}, {m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight()}};
  m_state_tracker->SetRenderPass(m_framebuffer_mgr->GetEFBRenderPass());
  m_state_tracker->SetFramebuffer(m_framebuffer_mgr->GetEFBFramebuffer(), framebuffer_render_area);

  if (!CreateSemaphores())
    return false;

  m_raster_font = std::make_unique<RasterFont>();
  if (!m_raster_font->Initialize())
    return false;

  // Initialize annoying statics
  s_last_efb_scale = g_ActiveConfig.iEFBScale;

  // Update backbuffer dimensions
  OnSwapChainResized();

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
  if ((res = vkCreateSemaphore(g_object_cache->GetDevice(), &semaphore_info, nullptr,
                               &m_image_available_semaphore)) != VK_SUCCESS ||
      (res = vkCreateSemaphore(g_object_cache->GetDevice(), &semaphore_info, nullptr,
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
    vkDestroySemaphore(g_object_cache->GetDevice(), m_image_available_semaphore, nullptr);
    m_image_available_semaphore = nullptr;
  }

  if (m_rendering_finished_semaphore)
  {
    vkDestroySemaphore(g_object_cache->GetDevice(), m_rendering_finished_semaphore, nullptr);
    m_rendering_finished_semaphore = nullptr;
  }
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
  u32 backbuffer_width = m_swap_chain->GetSize().width;
  u32 backbuffer_height = m_swap_chain->GetSize().height;

  m_raster_font->PrintMultiLineText(m_swap_chain->GetRenderPass(), text,
                                    left * 2.0f / static_cast<float>(backbuffer_width) - 1,
                                    1 - top * 2.0f / static_cast<float>(backbuffer_height),
                                    backbuffer_width, backbuffer_height, color);
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

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                           u32 color, u32 z)
{
  // Native -> EFB coordinates
  TargetRectangle target_rc = Renderer::ConvertEFBRectangle(rc);

  // Clearing must occur within a render pass.
  m_state_tracker->BeginRenderPass();

  // fast path: when both color and alpha are enabled, we can blow away the entire buffer
  // TODO: Can we also do it when the buffer is not an RGBA format?
  VkClearAttachment clear_attachments[2];
  uint32_t num_clear_attachments = 0;
  if (colorEnable && alphaEnable)
  {
    clear_attachments[num_clear_attachments].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_attachments[num_clear_attachments].colorAttachment = 0;
    clear_attachments[num_clear_attachments].clearValue.color.float32[0] =
        float((color >> 16) & 0xFF) / 255.0f;
    clear_attachments[num_clear_attachments].clearValue.color.float32[1] =
        float((color >> 8) & 0xFF) / 255.0f;
    clear_attachments[num_clear_attachments].clearValue.color.float32[2] =
        float((color >> 0) & 0xFF) / 255.0f;
    clear_attachments[num_clear_attachments].clearValue.color.float32[3] =
        float((color >> 24) & 0xFF) / 255.0f;
    num_clear_attachments++;
  }
  else
  {
    // Mask away the appropriate colors and use a shader
    BlendState blend_state = Util::GetNoBlendingBlendState();
    if (colorEnable)
      blend_state.write_mask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
    else
      blend_state.write_mask = VK_COLOR_COMPONENT_A_BIT;

    // No need to start a new render pass, but we do need to restore viewport state
    UtilityShaderDraw draw(g_object_cache->GetStandardPipelineLayout(),
                           m_framebuffer_mgr->GetEFBRenderPass(),
                           g_object_cache->GetSharedShaderCache().GetPassthroughVertexShader(),
                           g_object_cache->GetSharedShaderCache().GetPassthroughGeometryShader(),
                           g_object_cache->GetSharedShaderCache().GetClearFragmentShader());

    draw.SetBlendState(blend_state);
    draw.DrawColoredQuad(target_rc.left, target_rc.top, target_rc.GetWidth(), target_rc.GetHeight(),
                         float((color >> 16) & 0xFF) / 255.0f, float((color >> 8) & 0xFF) / 255.0f,
                         float((color >> 0) & 0xFF) / 255.0f, float((color >> 24) & 0xFF) / 255.0f);

    m_state_tracker->SetPendingRebind();
  }

  if (zEnable)
  {
    clear_attachments[num_clear_attachments].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clear_attachments[num_clear_attachments].colorAttachment = 0;
    clear_attachments[num_clear_attachments].clearValue.depthStencil.depth =
        1.0f - (float(z & 0xFFFFFF) / 16777216.0f);
    clear_attachments[num_clear_attachments].clearValue.depthStencil.stencil = 0;
    num_clear_attachments++;
  }

  if (num_clear_attachments > 0)
  {
    VkClearRect rect;
    rect.rect.offset = {target_rc.left, target_rc.top};
    rect.rect.extent = {static_cast<uint32_t>(target_rc.GetWidth()),
                        static_cast<uint32_t>(target_rc.GetHeight())};

    rect.baseArrayLayer = 0;
    rect.layerCount = m_framebuffer_mgr->GetEFBLayers();

    vkCmdClearAttachments(g_command_buffer_mgr->GetCurrentCommandBuffer(), num_clear_attachments,
                          clear_attachments, 1, &rect);
  }
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
  m_state_tracker->EndRenderPass();
  m_state_tracker->SetPendingRebind();
  m_framebuffer_mgr->ReinterpretPixelData(convtype);

  // EFB framebuffer has now changed, so update accordingly.
  VkRect2D framebuffer_render_area = {
      {0, 0}, {m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight()}};
  m_state_tracker->SetFramebuffer(m_framebuffer_mgr->GetEFBFramebuffer(), framebuffer_render_area);
}

void Renderer::SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height,
                        const EFBRectangle& rc, float gamma)
{
  // End the current render pass.
  m_state_tracker->EndRenderPass();

  UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

  // Transition the EFB render target to a shader resource.
  m_framebuffer_mgr->GetEFBColorTexture()->TransitionToLayout(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Ensure the worker thread is not still submitting a previous command buffer.
  // In other words, the last frame has been submitted (otherwise the next call would
  // be a race, as the image may not have been consumed yet).
  g_command_buffer_mgr->PrepareToSubmitCommandBuffer();

  // Grab the next image from the swap chain in preparation for drawing the window.
  VkResult res = m_swap_chain->AcquireNextImage(m_image_available_semaphore);
  if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
  {
    // Window has been resized. Update the swap chain and try again.
    // The resize here is safe since the worker thread is guaranteed to be idle.
    if (m_swap_chain->ResizeSwapChain())
    {
      res = m_swap_chain->AcquireNextImage(m_image_available_semaphore);
      OnSwapChainResized();
    }
  }
  if (res != VK_SUCCESS)
    PanicAlert("Failed to grab image from swap chain");

  // Transition from undefined (or present src, but it can be substituted) to
  // color attachment ready for writing.
  // These transitions must occur outside a render pass, unless the render pass
  // declares a self-dependency.
  m_swap_chain->GetCurrentTexture()->OverrideImageLayout(VK_IMAGE_LAYOUT_UNDEFINED);
  m_swap_chain->GetCurrentTexture()->TransitionToLayout(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Blit the EFB to the back buffer (Swap chain)
  UtilityShaderDraw draw(g_object_cache->GetStandardPipelineLayout(), m_swap_chain->GetRenderPass(),
                         g_object_cache->GetSharedShaderCache().GetPassthroughVertexShader(),
                         VK_NULL_HANDLE,
                         g_object_cache->GetSharedShaderCache().GetCopyFragmentShader());

  // Begin the present render pass
  VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  VkRect2D region = {{0, 0}, m_swap_chain->GetSize()};
  draw.BeginRenderPass(m_swap_chain->GetCurrentFramebuffer(), region, &clear_value);

  // Find the rect we want to draw into on the backbuffer
  TargetRectangle target_rc = GetTargetRectangle();
  TargetRectangle source_rc = Renderer::ConvertEFBRectangle(rc);

  // Draw a quad that blits/scales the internal framebuffer to the swap chain.
  draw.SetPSSampler(0, m_framebuffer_mgr->GetEFBColorTexture()->GetView(),
                    g_object_cache->GetLinearSampler());
  draw.DrawQuad(source_rc.left, source_rc.top, source_rc.GetWidth(), source_rc.GetHeight(),
                m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight(), target_rc.left,
                target_rc.top, target_rc.GetWidth(), target_rc.GetHeight());

  // OSD stuff
  Util::SetViewportAndScissor(g_command_buffer_mgr->GetCurrentCommandBuffer(), 0, 0,
                              m_swap_chain->GetSize().width, m_swap_chain->GetSize().height);
  DrawDebugText();

  // Do our OSD callbacks
  OSD::DoCallbacks(OSD::CallbackType::OnFrame);
  OSD::DrawMessages();

  // End drawing to backbuffer
  draw.EndRenderPass();

  // Transition the backbuffer to PRESENT_SRC to ensure all commands drawing to it have finished
  // before present.
  m_swap_chain->GetCurrentTexture()->TransitionToLayout(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // Submit the current command buffer, signaling rendering finished semaphore when it's done
  // Because this final command buffer is rendering to the swap chain, we need to wait for
  // the available semaphore to be signaled before executing the buffer. This final submission
  // can happen off-thread in the background while we're preparing the next frame.
  g_command_buffer_mgr->SubmitCommandBufferAndPresent(
      m_image_available_semaphore, m_rendering_finished_semaphore, m_swap_chain->GetSwapChain(),
      m_swap_chain->GetCurrentImageIndex(), true);

  // Prep for the next frame (get command buffer ready) before doing anything else.
  BeginFrame();

  // Restore the EFB color texture to color attachment ready for rendering.
  m_framebuffer_mgr->GetEFBColorTexture()->TransitionToLayout(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Clean up stale textures
  TextureCacheBase::Cleanup(frameCount);

  // Determine what has changed in the config.
  CheckForSurfaceChange();
  CheckForConfigChanges();

#ifdef VK_USE_PLATFORM_WIN32_KHR
  // For some reason, the windows WSI (at least on nVidia) doesn't return VK_ERROR_OUT_OF_DATE_KHR.
  // Horrible work around for this, we should really catch WM_SIZE.
  {
    RECT client_rect;
    if (GetClientRect(reinterpret_cast<HWND>(m_swap_chain->GetNativeHandle()), &client_rect))
    {
      LONG width = client_rect.right - client_rect.left;
      LONG height = client_rect.bottom - client_rect.top;
      if (width != static_cast<LONG>(m_swap_chain->GetSize().width) ||
          height != static_cast<LONG>(m_swap_chain->GetSize().height))
      {
        WARN_LOG(VIDEO, "Detected client rect change, resizing swap chain");
        ResizeSwapChain();
      }
    }
  }
#endif
}

void Renderer::CheckForConfigChanges()
{
  // Compare g_Config to g_ActiveConfig to determine what has changed before copying.
  bool vsync_changed = (g_Config.bVSync != g_ActiveConfig.bVSync);
  // bool msaa_changed = (g_Config.iMultisamples != g_ActiveConfig.iMultisamples);
  // bool ssaa_changed = (g_Config.bSSAA != g_ActiveConfig.bSSAA);
  bool anisotropy_changed = (g_Config.iMaxAnisotropy != g_ActiveConfig.iMaxAnisotropy);
  bool force_texture_filtering_changed =
      (g_Config.bForceFiltering != g_ActiveConfig.bForceFiltering);
  // bool stereo_changed = (g_Config.iStereoMode != g_ActiveConfig.iStereoMode);

  // Copy g_Config to g_ActiveConfig.
  UpdateActiveConfig();

  // Handle internal resolution changes.
  if (s_last_efb_scale != g_ActiveConfig.iEFBScale)
  {
    s_last_efb_scale = g_ActiveConfig.iEFBScale;
    if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height))
      ResizeEFBTextures();
  }

  // For vsync, we need to change the present mode, which means recreating the swap chain.
  if (vsync_changed)
    ResizeSwapChain();

  // Wipe sampler cache if force texture filtering or anisotropy changes.
  if (anisotropy_changed || force_texture_filtering_changed)
    ResetSamplerStates();
}

void Renderer::CheckForSurfaceChange()
{
  if (!s_SurfaceNeedsChanged.IsSet())
    return;

  // Wait for the GPU to catch up
  g_command_buffer_mgr->WaitForGPUIdle();

  // Recreate the surface. If this fails we're in trouble.
  VkExtent2D old_size = m_swap_chain->GetSize();
  if (!m_swap_chain->RecreateSurface(m_new_window_handle))
    PanicAlert("Failed to recreate Vulkan surface. Cannot continue.");

  // Notify calling thread.
  m_new_window_handle = nullptr;
  s_SurfaceNeedsChanged.Clear();
  s_ChangedSurface.Set();

  // Handle case where the dimensions are now different
  if (old_size.width != m_swap_chain->GetSize().width ||
      old_size.height != m_swap_chain->GetSize().height)
  {
    OnSwapChainResized();
  }
}

void Renderer::OnSwapChainResized()
{
  s_backbuffer_width = m_swap_chain->GetSize().width;
  s_backbuffer_height = m_swap_chain->GetSize().height;
  FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
  FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);
  UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
  if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height))
    ResizeEFBTextures();

  PixelShaderManager::SetEfbScaleChanged();
}

void Renderer::ResizeEFBTextures()
{
  // Ensure the GPU is finished with the current EFB textures.
  g_command_buffer_mgr->WaitForGPUIdle();

  m_framebuffer_mgr->ResizeEFBTextures();
  s_last_efb_scale = g_ActiveConfig.iEFBScale;

  // Update framebuffer in state tracker
  VkRect2D framebuffer_render_area = {
      {0, 0}, {m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight()}};
  m_state_tracker->SetRenderPass(m_framebuffer_mgr->GetEFBRenderPass());
  m_state_tracker->SetFramebuffer(m_framebuffer_mgr->GetEFBFramebuffer(), framebuffer_render_area);

  // Viewport and scissor rect have to be reset since they will be scaled differently.
  SetViewport();
  BPFunctions::SetScissor();
}

void Renderer::ResizeSwapChain()
{
  // The worker thread may still be submitting a present on this swap chain.
  g_command_buffer_mgr->WaitForGPUIdle();

  // It's now safe to resize the swap chain.
  m_swap_chain->ResizeSwapChain();
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
  new_ds_state.test_enable = (bpmem.zmode.testenable) ? VK_TRUE : VK_FALSE;
  new_ds_state.write_enable = (bpmem.zmode.updateenable) ? VK_TRUE : VK_FALSE;

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
  new_blend_state.hex = m_state_tracker->GetBlendState().hex;
  new_blend_state.write_mask = color_mask;

  m_state_tracker->SetBlendState(new_blend_state);
}

void Renderer::SetBlendMode(bool forceUpdate)
{
  BlendState new_blend_state = {};
  new_blend_state.hex = m_state_tracker->GetBlendState().hex;

  // Fast path for blending disabled
  if (!bpmem.blendmode.blendenable)
  {
    new_blend_state.blend_enable = VK_FALSE;
    new_blend_state.blend_op = VK_BLEND_OP_ADD;
    new_blend_state.src_blend = VK_BLEND_FACTOR_ONE;
    new_blend_state.dst_blend = VK_BLEND_FACTOR_ZERO;
    new_blend_state.use_dst_alpha = VK_FALSE;
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
    new_blend_state.use_dst_alpha = VK_FALSE;
    m_state_tracker->SetBlendState(new_blend_state);
    return;
  }

  // Our render target always uses an alpha channel, so we need to override the blend functions to
  // assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
  // Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel
  // is assumed to always be 1.
  bool target_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
  bool use_dst_alpha = bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate && target_has_alpha;
  if (!g_object_cache->SupportsDualSourceBlend())
    use_dst_alpha = false;

  new_blend_state.blend_enable = VK_TRUE;
  new_blend_state.blend_op = VK_BLEND_OP_ADD;
  new_blend_state.use_dst_alpha = use_dst_alpha ? VK_TRUE : VK_FALSE;

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
    new_blend_state.src_blend = VK_BLEND_FACTOR_SRC_ALPHA;
    break;
  case BlendMode::INVSRCALPHA:
    new_blend_state.src_blend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    break;
  case BlendMode::DSTALPHA:
    new_blend_state.src_blend =
        (target_has_alpha) ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_ONE;
    break;
  case BlendMode::INVDSTALPHA:
    new_blend_state.src_blend =
        (target_has_alpha) ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ZERO;
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
    new_blend_state.dst_blend = VK_BLEND_FACTOR_SRC_ALPHA;
    break;
  case BlendMode::INVSRCALPHA:
    new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    break;
  case BlendMode::DSTALPHA:
    new_blend_state.dst_blend =
        (target_has_alpha) ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_ONE;
    break;
  case BlendMode::INVDSTALPHA:
    new_blend_state.dst_blend =
        (target_has_alpha) ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ZERO;
    break;
  default:
    new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE;
    break;
  }

  m_state_tracker->SetBlendState(new_blend_state);
}

void Renderer::SetLogicOpMode()
{
  BlendState new_blend_state = {};
  new_blend_state.hex = m_state_tracker->GetBlendState().hex;

  // Does our device support logic ops?
  bool logic_op_enable = bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable;
  if (g_object_cache->SupportsLogicOps())
  {
    if (logic_op_enable)
    {
      static const std::array<VkLogicOp, 16> logic_ops = {
          VK_LOGIC_OP_CLEAR,         VK_LOGIC_OP_AND,
          VK_LOGIC_OP_AND_REVERSE,   VK_LOGIC_OP_COPY,
          VK_LOGIC_OP_AND_INVERTED,  VK_LOGIC_OP_NO_OP,
          VK_LOGIC_OP_XOR,           VK_LOGIC_OP_OR,
          VK_LOGIC_OP_NOR,           VK_LOGIC_OP_EQUIVALENT,
          VK_LOGIC_OP_INVERT,        VK_LOGIC_OP_OR_REVERSE,
          VK_LOGIC_OP_COPY_INVERTED, VK_LOGIC_OP_OR_INVERTED,
          VK_LOGIC_OP_NAND,          VK_LOGIC_OP_SET};

      new_blend_state.logic_op_enable = VK_TRUE;
      new_blend_state.logic_op = logic_ops[bpmem.blendmode.logicmode];
    }
    else
    {
      new_blend_state.logic_op_enable = VK_FALSE;
      new_blend_state.logic_op = VK_LOGIC_OP_NO_OP;
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

      new_blend_state.blend_enable = VK_FALSE;
      new_blend_state.src_blend = logic_ops[bpmem.blendmode.logicmode].src_factor;
      new_blend_state.dst_blend = logic_ops[bpmem.blendmode.logicmode].dst_factor;
      new_blend_state.blend_op = logic_ops[bpmem.blendmode.logicmode].op;
      new_blend_state.use_dst_alpha = 0;
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
    new_state.min_filter = ((tm0.min_filter & 4) != 0) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    new_state.mipmap_mode = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ?
                                VK_SAMPLER_MIPMAP_MODE_LINEAR :
                                VK_SAMPLER_MIPMAP_MODE_NEAREST;
    new_state.mag_filter = (tm0.mag_filter != 0) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  }

  // If mipmaps are disabled, clamp min/max lod
  new_state.max_lod = (SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0)) ?
                          static_cast<u32>(MathUtil::Clamp(tm1.max_lod / 16.0f, 0.0f, 255.0f)) :
                          0;
  new_state.min_lod =
      std::min(new_state.max_lod.Value(),
               static_cast<u32>(MathUtil::Clamp(tm1.min_lod / 16.0f, 0.0f, 255.0f)));
  new_state.lod_bias = static_cast<s32>(tm0.lod_bias / 32.0f);

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
  if (g_object_cache->SupportsAnisotropicFiltering() && g_ActiveConfig.iMaxAnisotropy > 0 &&
      !SamplerCommon::IsBpTexMode0PointFiltering(tm0))
  {
    new_state.anisotropy = g_ActiveConfig.iMaxAnisotropy;
  }
  else
  {
    new_state.anisotropy = 0;
  }

  // Skip lookup if the state hasn't changed.
  size_t bind_index = (texindex * 4) + stage;
  if (m_sampler_states[bind_index].hex == new_state.hex)
    return;

  // Look up new state and replace in state tracker.
  VkSampler sampler = g_object_cache->GetSampler(new_state);
  if (sampler == VK_NULL_HANDLE)
  {
    ERROR_LOG(VIDEO, "Failed to create sampler");
    sampler = g_object_cache->GetPointSampler();
  }

  m_state_tracker->SetSampler(bind_index, sampler);
  m_sampler_states[bind_index].hex = new_state.hex;
}

void Renderer::ResetSamplerStates()
{
  // Ensure none of the sampler objects are in use.
  g_command_buffer_mgr->WaitForGPUIdle();

  // Invalidate all sampler states, next draw will re-initialize them.
  for (size_t i = 0; i < m_sampler_states.size(); i++)
  {
    m_sampler_states[i].hex = std::numeric_limits<decltype(m_sampler_states[i].hex)>::max();
    m_state_tracker->SetSampler(i, g_object_cache->GetPointSampler());
  }

  // Invalidate all sampler objects (some will be unused now).
  g_object_cache->ClearSamplerCache();
}

void Renderer::SetDitherMode()
{
  // TODO: Implement me
}

void Renderer::SetInterlacingMode()
{
  // TODO: Implement me
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

  // Use reversed depth here
  float min_depth =
      1.0f - MathUtil::Clamp<float>(xfmem.viewport.farZ, 0.0f, 16777215.0f) / 16777216.0f;
  float max_depth =
      1.0f -
      MathUtil::Clamp<float>(xfmem.viewport.farZ -
                                 MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777216.0f),
                             0.0f, 16777215.0f) /
          16777216.0f;

  VkViewport viewport = {x, y, width, height, min_depth, max_depth};
  m_state_tracker->SetViewport(viewport);
}

void Renderer::ChangeSurface(void* new_window_handle)
{
  m_new_window_handle = new_window_handle;
  s_SurfaceNeedsChanged.Set();
  s_ChangedSurface.Wait();
}

}  // namespace Vulkan
