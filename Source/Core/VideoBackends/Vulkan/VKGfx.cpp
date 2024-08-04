// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/VKGfx.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>
#include <tuple>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/EnumUtils.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/VKPipeline.h"
#include "VideoBackends/Vulkan/VKShader.h"
#include "VideoBackends/Vulkan/VKSwapChain.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VKVertexFormat.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
VKGfx::VKGfx(std::unique_ptr<SwapChain> swap_chain, float backbuffer_scale)
    : m_swap_chain(std::move(swap_chain)), m_backbuffer_scale(backbuffer_scale)
{
  UpdateActiveConfig();
  for (SamplerState& sampler_state : m_sampler_states)
    sampler_state = RenderState::GetPointSamplerState();

  // Various initialization routines will have executed commands on the command buffer.
  // Execute what we have done before beginning the first frame.
  ExecuteCommandBuffer(true, false);
}

VKGfx::~VKGfx() = default;

bool VKGfx::IsHeadless() const
{
  return m_swap_chain == nullptr;
}

std::unique_ptr<AbstractTexture> VKGfx::CreateTexture(const TextureConfig& config,
                                                      std::string_view name)
{
  return VKTexture::Create(config, name);
}

std::unique_ptr<AbstractStagingTexture> VKGfx::CreateStagingTexture(StagingTextureType type,
                                                                    const TextureConfig& config)
{
  return VKStagingTexture::Create(type, config);
}

std::unique_ptr<AbstractShader>
VKGfx::CreateShaderFromSource(ShaderStage stage, std::string_view source, std::string_view name)
{
  return VKShader::CreateFromSource(stage, source, name);
}

std::unique_ptr<AbstractShader> VKGfx::CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                              size_t length, std::string_view name)
{
  return VKShader::CreateFromBinary(stage, data, length, name);
}

std::unique_ptr<NativeVertexFormat>
VKGfx::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<VertexFormat>(vtx_decl);
}

std::unique_ptr<AbstractPipeline> VKGfx::CreatePipeline(const AbstractPipelineConfig& config,
                                                        const void* cache_data,
                                                        size_t cache_data_length)
{
  return VKPipeline::Create(config);
}

std::unique_ptr<AbstractFramebuffer>
VKGfx::CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                         std::vector<AbstractTexture*> additional_color_attachments)
{
  return VKFramebuffer::Create(static_cast<VKTexture*>(color_attachment),
                               static_cast<VKTexture*>(depth_attachment),
                               std::move(additional_color_attachments));
}

void VKGfx::SetPipeline(const AbstractPipeline* pipeline)
{
  StateTracker::GetInstance()->SetPipeline(static_cast<const VKPipeline*>(pipeline));
}

void VKGfx::ClearRegion(const MathUtil::Rectangle<int>& target_rc, bool color_enable,
                        bool alpha_enable, bool z_enable, u32 color, u32 z)
{
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
  clear_depth_value.depthStencil.depth = static_cast<float>(z & 0xFFFFFF) / 16777216.0f;
  if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
    clear_depth_value.depthStencil.depth = 1.0f - clear_depth_value.depthStencil.depth;

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

  auto* vk_frame_buffer = static_cast<VKFramebuffer*>(m_current_framebuffer);

  // Fastest path: Use a render pass to clear the buffers.
  if (use_clear_render_pass)
  {
    vk_frame_buffer->SetAndClear(target_vk_rc, clear_color_value, clear_depth_value);
    return;
  }

  // Fast path: Use vkCmdClearAttachments to clear the buffers within a render path
  // We can't use this when preserving alpha but clearing color.
  if (use_clear_attachments)
  {
    std::vector<VkClearAttachment> clear_attachments;
    bool has_color = false;
    if (color_enable && alpha_enable)
    {
      VkClearAttachment clear_attachment;
      clear_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      clear_attachment.colorAttachment = 0;
      clear_attachment.clearValue = clear_color_value;
      clear_attachments.push_back(std::move(clear_attachment));
      color_enable = false;
      alpha_enable = false;
      has_color = true;
    }
    if (z_enable)
    {
      VkClearAttachment clear_attachment;
      clear_attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      clear_attachment.colorAttachment = 0;
      clear_attachment.clearValue = clear_depth_value;
      clear_attachments.push_back(std::move(clear_attachment));
      z_enable = false;
    }
    if (has_color)
    {
      for (std::size_t i = 0; i < vk_frame_buffer->GetNumberOfAdditonalAttachments(); i++)
      {
        VkClearAttachment clear_attachment;
        clear_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clear_attachment.colorAttachment = 0;
        clear_attachment.clearValue = clear_color_value;
        clear_attachments.push_back(std::move(clear_attachment));
      }
    }
    if (!clear_attachments.empty())
    {
      VkClearRect vk_rect = {target_vk_rc, 0, g_framebuffer_manager->GetEFBLayers()};
      if (!StateTracker::GetInstance()->IsWithinRenderArea(
              target_vk_rc.offset.x, target_vk_rc.offset.y, target_vk_rc.extent.width,
              target_vk_rc.extent.height))
      {
        StateTracker::GetInstance()->EndClearRenderPass();
      }
      StateTracker::GetInstance()->BeginRenderPass();

      vkCmdClearAttachments(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                            static_cast<uint32_t>(clear_attachments.size()),
                            clear_attachments.data(), 1, &vk_rect);
    }
  }

  // Anything left over for the slow path?
  if (!color_enable && !alpha_enable && !z_enable)
    return;

  AbstractGfx::ClearRegion(target_rc, color_enable, alpha_enable, z_enable, color, z);

  EmitDebugMarker(VkDebugCommand::ClearRegion);
}

void VKGfx::Flush()
{
  ExecuteCommandBuffer(true, false);
}

void VKGfx::WaitForGPUIdle()
{
  ExecuteCommandBuffer(false, true);
}

void VKGfx::BindBackbuffer(const ClearColor& clear_color)
{
  StateTracker::GetInstance()->EndRenderPass();

  if (!g_command_buffer_mgr->CheckLastPresentDone())
    g_command_buffer_mgr->WaitForWorkerThreadIdle();

  // Handle host window resizes.
  CheckForSurfaceChange();
  CheckForSurfaceResize();

  // Check for exclusive fullscreen request.
  if (m_swap_chain->GetCurrentFullscreenState() != m_swap_chain->GetNextFullscreenState() &&
      !m_swap_chain->SetFullscreenState(m_swap_chain->GetNextFullscreenState()))
  {
    // if it fails, don't keep trying
    m_swap_chain->SetNextFullscreenState(m_swap_chain->GetCurrentFullscreenState());
  }

  const bool present_fail = g_command_buffer_mgr->CheckLastPresentFail();
  VkResult res = present_fail ? g_command_buffer_mgr->GetLastPresentResult() :
                                m_swap_chain->AcquireNextImage();

  if (res == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT &&
      !m_swap_chain->GetCurrentFullscreenState())
  {
    // AMD's binary driver as of 21.3 seems to return exclusive fullscreen lost even when it was
    // never requested, so long as the caller requested it to be application controlled. Handle
    // this ignoring the lost result and just continuing as normal if we never acquired it.
    res = VK_SUCCESS;
    if (present_fail)
    {
      // We still need to acquire an image.
      res = m_swap_chain->AcquireNextImage();
    }
  }

  if (res != VK_SUCCESS)
  {
    // Execute cmdbuffer before resizing, as the last frame could still be presenting.
    ExecuteCommandBuffer(false, true);

    // Was this a lost exclusive fullscreen?
    if (res == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
    {
      // The present keeps returning exclusive mode lost unless we re-create the swap chain.
      INFO_LOG_FMT(VIDEO, "Lost exclusive fullscreen.");
      m_swap_chain->RecreateSwapChain();
    }
    else if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
    {
      INFO_LOG_FMT(VIDEO, "Resizing swap chain due to suboptimal/out-of-date");
      m_swap_chain->ResizeSwapChain();
    }
    else
    {
      ERROR_LOG_FMT(VIDEO, "Unknown present error {:#010X} {}, please report.",
                    Common::ToUnderlying(res), VkResultToString(res));
      m_swap_chain->RecreateSwapChain();
    }

    res = m_swap_chain->AcquireNextImage();
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
      PanicAlertFmt("Failed to grab image from swap chain: {:#010X} {}", Common::ToUnderlying(res),
                    VkResultToString(res));
  }

  // Transition from undefined (or present src, but it can be substituted) to
  // color attachment ready for writing. These transitions must occur outside
  // a render pass, unless the render pass declares a self-dependency.
  m_swap_chain->GetCurrentTexture()->OverrideImageLayout(VK_IMAGE_LAYOUT_UNDEFINED);
  m_swap_chain->GetCurrentTexture()->TransitionToLayout(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  SetAndClearFramebuffer(m_swap_chain->GetCurrentFramebuffer(),
                         ClearColor{{0.0f, 0.0f, 0.0f, 1.0f}});
}

void VKGfx::PresentBackbuffer()
{
  // End drawing to backbuffer
  StateTracker::GetInstance()->EndRenderPass();

  // Transition the backbuffer to PRESENT_SRC to ensure all commands drawing
  // to it have finished before present.
  m_swap_chain->GetCurrentTexture()->TransitionToLayout(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // Submit the current command buffer, signaling rendering finished semaphore when it's done
  // Because this final command buffer is rendering to the swap chain, we need to wait for
  // the available semaphore to be signaled before executing the buffer. This final submission
  // can happen off-thread in the background while we're preparing the next frame.
  g_command_buffer_mgr->SubmitCommandBuffer(true, false, m_swap_chain->GetSwapChain(),
                                            m_swap_chain->GetCurrentImageIndex());

  // New cmdbuffer, so invalidate state.
  StateTracker::GetInstance()->InvalidateCachedState();
}

void VKGfx::SetFullscreen(bool enable_fullscreen)
{
  if (!m_swap_chain->IsFullscreenSupported())
    return;

  m_swap_chain->SetNextFullscreenState(enable_fullscreen);
}

bool VKGfx::IsFullscreen() const
{
  return m_swap_chain && m_swap_chain->GetCurrentFullscreenState();
}

void VKGfx::ExecuteCommandBuffer(bool submit_off_thread, bool wait_for_completion)
{
  StateTracker::GetInstance()->EndRenderPass();

  g_command_buffer_mgr->SubmitCommandBuffer(submit_off_thread, wait_for_completion);

  StateTracker::GetInstance()->InvalidateCachedState();
}

void VKGfx::CheckForSurfaceChange()
{
  if (!g_presenter->SurfaceChangedTestAndClear() || !m_swap_chain)
    return;

  // Submit the current draws up until rendering the XFB.
  ExecuteCommandBuffer(false, true);

  // Clear the present failed flag, since we don't want to resize after recreating.
  g_command_buffer_mgr->CheckLastPresentFail();

  // Recreate the surface. If this fails we're in trouble.
  if (!m_swap_chain->RecreateSurface(g_presenter->GetNewSurfaceHandle()))
    PanicAlertFmt("Failed to recreate Vulkan surface. Cannot continue.");

  // Handle case where the dimensions are now different.
  OnSwapChainResized();
}

void VKGfx::CheckForSurfaceResize()
{
  if (!g_presenter->SurfaceResizedTestAndClear())
    return;

  // If we don't have a surface, how can we resize the swap chain?
  // CheckForSurfaceChange should handle this case.
  if (!m_swap_chain)
  {
    WARN_LOG_FMT(VIDEO, "Surface resize event received without active surface, ignoring");
    return;
  }

  // Wait for the GPU to catch up since we're going to destroy the swap chain.
  ExecuteCommandBuffer(false, true);

  // Clear the present failed flag, since we don't want to resize after recreating.
  g_command_buffer_mgr->CheckLastPresentFail();

  // Resize the swap chain.
  m_swap_chain->RecreateSwapChain();
  OnSwapChainResized();
}

void VKGfx::OnConfigChanged(u32 bits)
{
  AbstractGfx::OnConfigChanged(bits);

  if (bits & CONFIG_CHANGE_BIT_HOST_CONFIG)
    g_object_cache->ReloadPipelineCache();

  // For vsync, we need to change the present mode, which means recreating the swap chain.
  if (m_swap_chain && (bits & CONFIG_CHANGE_BIT_VSYNC))
  {
    ExecuteCommandBuffer(false, true);
    m_swap_chain->SetVSync(g_ActiveConfig.bVSyncActive);
  }

  // For quad-buffered stereo we need to change the layer count, so recreate the swap chain.
  if (m_swap_chain && ((bits & CONFIG_CHANGE_BIT_STEREO_MODE) || (bits & CONFIG_CHANGE_BIT_HDR)))
  {
    ExecuteCommandBuffer(false, true);
    m_swap_chain->RecreateSwapChain();
  }

  // Wipe sampler cache if force texture filtering or anisotropy changes.
  if (bits & (CONFIG_CHANGE_BIT_ANISOTROPY | CONFIG_CHANGE_BIT_FORCE_TEXTURE_FILTERING))
  {
    ExecuteCommandBuffer(false, true);
    ResetSamplerStates();
  }
}

void VKGfx::OnSwapChainResized()
{
  g_presenter->SetBackbuffer(m_swap_chain->GetWidth(), m_swap_chain->GetHeight());
}

void VKGfx::BindFramebuffer(VKFramebuffer* fb)
{
  StateTracker::GetInstance()->EndRenderPass();

  // Shouldn't be bound as a texture.
  fb->Unbind();

  fb->TransitionForRender();
  StateTracker::GetInstance()->SetFramebuffer(fb);
  m_current_framebuffer = fb;
}

void VKGfx::SetFramebuffer(AbstractFramebuffer* framebuffer)
{
  if (m_current_framebuffer == framebuffer)
    return;

  VKFramebuffer* vkfb = static_cast<VKFramebuffer*>(framebuffer);
  BindFramebuffer(vkfb);
}

void VKGfx::SetAndDiscardFramebuffer(AbstractFramebuffer* framebuffer)
{
  if (m_current_framebuffer == framebuffer)
    return;

  VKFramebuffer* vkfb = static_cast<VKFramebuffer*>(framebuffer);
  BindFramebuffer(vkfb);

  // If we're discarding, begin the discard pass, then switch to a load pass.
  // This way if the command buffer is flushed, we don't start another discard pass.
  StateTracker::GetInstance()->BeginDiscardRenderPass();
}

void VKGfx::SetAndClearFramebuffer(AbstractFramebuffer* framebuffer, const ClearColor& color_value,
                                   float depth_value)
{
  VKFramebuffer* vkfb = static_cast<VKFramebuffer*>(framebuffer);
  BindFramebuffer(vkfb);

  VkClearValue clear_color_value;
  std::memcpy(clear_color_value.color.float32, color_value.data(),
              sizeof(clear_color_value.color.float32));
  VkClearValue clear_depth_value;
  clear_depth_value.depthStencil.depth = depth_value;
  clear_depth_value.depthStencil.stencil = 0;
  vkfb->SetAndClear(vkfb->GetRect(), clear_color_value, clear_depth_value);
}

void VKGfx::SetTexture(u32 index, const AbstractTexture* texture)
{
  // Texture should always be in SHADER_READ_ONLY layout prior to use.
  // This is so we don't need to transition during render passes.
  const VKTexture* tex = static_cast<const VKTexture*>(texture);
  if (tex)
  {
    if (tex->GetLayout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
      if (StateTracker::GetInstance()->InRenderPass())
      {
        WARN_LOG_FMT(VIDEO, "Transitioning image in render pass in VKGfx::SetTexture()");
        StateTracker::GetInstance()->EndRenderPass();
      }

      tex->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    StateTracker::GetInstance()->SetTexture(index, tex->GetView());
  }
  else
  {
    StateTracker::GetInstance()->SetTexture(0, VK_NULL_HANDLE);
  }
}

void VKGfx::SetSamplerState(u32 index, const SamplerState& state)
{
  // Skip lookup if the state hasn't changed.
  if (m_sampler_states[index] == state)
    return;

  // Look up new state and replace in state tracker.
  VkSampler sampler = g_object_cache->GetSampler(state);
  if (sampler == VK_NULL_HANDLE)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to create sampler");
    sampler = g_object_cache->GetPointSampler();
  }

  StateTracker::GetInstance()->SetSampler(index, sampler);
  m_sampler_states[index] = state;
}

void VKGfx::SetComputeImageTexture(u32 index, AbstractTexture* texture, bool read, bool write)
{
  VKTexture* vk_texture = static_cast<VKTexture*>(texture);
  if (vk_texture)
  {
    StateTracker::GetInstance()->EndRenderPass();
    StateTracker::GetInstance()->SetImageTexture(index, vk_texture->GetView());
    vk_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                   read ? (write ? VKTexture::ComputeImageLayout::ReadWrite :
                                                   VKTexture::ComputeImageLayout::ReadOnly) :
                                          VKTexture::ComputeImageLayout::WriteOnly);
  }
  else
  {
    StateTracker::GetInstance()->SetImageTexture(index, VK_NULL_HANDLE);
  }
}

void VKGfx::UnbindTexture(const AbstractTexture* texture)
{
  StateTracker::GetInstance()->UnbindTexture(static_cast<const VKTexture*>(texture)->GetView());
}

void VKGfx::ResetSamplerStates()
{
  // Invalidate all sampler states, next draw will re-initialize them.
  for (u32 i = 0; i < m_sampler_states.size(); i++)
  {
    m_sampler_states[i] = RenderState::GetPointSamplerState();
    StateTracker::GetInstance()->SetSampler(i, g_object_cache->GetPointSampler());
  }

  // Invalidate all sampler objects (some will be unused now).
  g_object_cache->ClearSamplerCache();
}

void VKGfx::SetScissorRect(const MathUtil::Rectangle<int>& rc)
{
  VkRect2D scissor = {{rc.left, rc.top},
                      {static_cast<u32>(rc.GetWidth()), static_cast<u32>(rc.GetHeight())}};

  // See Vulkan spec for vkCmdSetScissor:
  // The x and y members of offset must be greater than or equal to 0.
  if (scissor.offset.x < 0)
  {
    scissor.extent.width -= -scissor.offset.x;
    scissor.offset.x = 0;
  }
  if (scissor.offset.y < 0)
  {
    scissor.extent.height -= -scissor.offset.y;
    scissor.offset.y = 0;
  }
  StateTracker::GetInstance()->SetScissor(scissor);
}

void VKGfx::SetViewport(float x, float y, float width, float height, float near_depth,
                        float far_depth)
{
  VkViewport viewport = {x, y, width, height, near_depth, far_depth};
  StateTracker::GetInstance()->SetViewport(viewport);
}

void VKGfx::Draw(u32 base_vertex, u32 num_vertices)
{
  if (!StateTracker::GetInstance()->Bind())
    return;

  vkCmdDraw(g_command_buffer_mgr->GetCurrentCommandBuffer(), num_vertices, 1, base_vertex, 0);

  const VKPipeline* pipeline = StateTracker::GetInstance()->GetPipeline();
  EmitDebugMarker(VkDebugCommand::Draw, pipeline->GetVSHash(), pipeline->GetPSHash(),
                  pipeline->GetGSHash(), num_vertices);
}

void VKGfx::DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex)
{
  if (!StateTracker::GetInstance()->Bind())
    return;

  vkCmdDrawIndexed(g_command_buffer_mgr->GetCurrentCommandBuffer(), num_indices, 1, base_index,
                   base_vertex, 0);

  const VKPipeline* pipeline = StateTracker::GetInstance()->GetPipeline();
  EmitDebugMarker(VkDebugCommand::DrawIndexed, pipeline->GetVSHash(), pipeline->GetPSHash(),
                  pipeline->GetGSHash(), num_indices);
}

void VKGfx::DispatchComputeShader(const AbstractShader* shader, u32 groupsize_x, u32 groupsize_y,
                                  u32 groupsize_z, u32 groups_x, u32 groups_y, u32 groups_z)
{
  StateTracker::GetInstance()->SetComputeShader(static_cast<const VKShader*>(shader));
  if (StateTracker::GetInstance()->BindCompute())
    vkCmdDispatch(g_command_buffer_mgr->GetCurrentCommandBuffer(), groups_x, groups_y, groups_z);

  EmitDebugMarker(VkDebugCommand::Dispatch, 0, groups_x, groups_y, groups_z);
}

SurfaceInfo VKGfx::GetSurfaceInfo() const
{
  return {m_swap_chain ? m_swap_chain->GetWidth() : 1u,
          m_swap_chain ? m_swap_chain->GetHeight() : 0u, m_backbuffer_scale,
          m_swap_chain ? m_swap_chain->GetTextureFormat() : AbstractTextureFormat::Undefined};
}

}  // namespace Vulkan
