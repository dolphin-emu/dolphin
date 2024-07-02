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
#include "Common/SmallVector.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/VKPipeline.h"
#include "VideoBackends/Vulkan/VKShader.h"
#include "VideoBackends/Vulkan/VKSwapChain.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VKVertexFormat.h"

#include "VKScheduler.h"
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
  g_scheduler->Record([c_pipeline = static_cast<const VKPipeline*>(pipeline)](
                          CommandBufferManager* command_buffer_mgr) {
    command_buffer_mgr->GetStateTracker()->SetPipeline(c_pipeline);
  });
}

void VKGfx::ClearRegion(const MathUtil::Rectangle<int>& target_rc, bool color_enable,
                        bool alpha_enable, bool z_enable, u32 color, u32 z)
{
  auto* vk_frame_buffer = static_cast<VKFramebuffer*>(m_current_framebuffer);

  g_scheduler->Record(
      [color, z, color_enable, alpha_enable, z_enable, target_rc,
       c_fb_color_format = vk_frame_buffer->GetColorFormat(),
       c_fb_depth_format = vk_frame_buffer->GetDepthFormat(),
       c_num_additional_color_attachments = vk_frame_buffer->GetNumberOfAdditonalAttachments()](
          CommandBufferManager* command_buffer_mgr) mutable {
        VkRect2D target_vk_rc = {{target_rc.left, target_rc.top},
                                 {static_cast<uint32_t>(target_rc.GetWidth()),
                                  static_cast<uint32_t>(target_rc.GetHeight())}};

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
        bool use_clear_render_pass = !command_buffer_mgr->GetStateTracker()->InRenderPass() &&
                                     color_enable && alpha_enable && z_enable;

        // The NVIDIA Vulkan driver causes the GPU to lock up, or throw exceptions if MSAA is
        // enabled, a non-full clear rect is specified, and a clear loadop or vkCmdClearAttachments
        // is used.
        if (g_ActiveConfig.iMultisamples > 1 &&
            DriverDetails::HasBug(DriverDetails::BUG_BROKEN_MSAA_CLEAR))
        {
          use_clear_render_pass = false;
          use_clear_attachments = false;
        }

        // This path cannot be used if the driver implementation doesn't guarantee pixels with no
        // drawn geometry in "this" renderpass won't be cleared
        if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_CLEAR_LOADOP_RENDERPASS))
          use_clear_render_pass = false;

        // Fastest path: Use a render pass to clear the buffers.
        if (use_clear_render_pass)
        {
          std::vector<VkClearValue> clear_values;
          if (c_fb_color_format != AbstractTextureFormat::Undefined)
          {
            clear_values.push_back(clear_color_value);
          }
          if (c_fb_depth_format != AbstractTextureFormat::Undefined)
          {
            clear_values.push_back(clear_depth_value);
          }
          for (std::size_t i = 0; i < c_num_additional_color_attachments; i++)
          {
            clear_values.push_back(clear_color_value);
          }

          command_buffer_mgr->GetStateTracker()->BeginClearRenderPass(
              target_vk_rc, clear_values.data(), static_cast<u32>(clear_values.size()));
          return;
        }

        // Fast path: Use vkCmdClearAttachments to clear the buffers within a render path
        // We can't use this when preserving alpha but clearing color.
        if (use_clear_attachments)
        {
          Common::SmallVector<VkClearAttachment, 4> clear_attachments;
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
            for (std::size_t i = 0; i < c_num_additional_color_attachments; i++)
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
            if (!command_buffer_mgr->GetStateTracker()->IsWithinRenderArea(
                    target_vk_rc.offset.x, target_vk_rc.offset.y, target_vk_rc.extent.width,
                    target_vk_rc.extent.height))
            {
              command_buffer_mgr->GetStateTracker()->EndClearRenderPass();
            }
            command_buffer_mgr->GetStateTracker()->BeginRenderPass();

            vkCmdClearAttachments(command_buffer_mgr->GetCurrentCommandBuffer(),
                                  static_cast<uint32_t>(clear_attachments.size()),
                                  clear_attachments.data(), 1, &vk_rect);
          }
        }
      });

  // Anything left over for the slow path?
  if (!color_enable && !alpha_enable && !z_enable)
    return;

  AbstractGfx::ClearRegion(target_rc, color_enable, alpha_enable, z_enable, color, z);
}

void VKGfx::Flush(FlushType flushType)
{
  if (flushType == FlushType::FlushToWorker)
  {
    g_scheduler->Flush();
  }
  else
  {
    ExecuteCommandBuffer(true, false);
  }
}

void VKGfx::WaitForGPUIdle()
{
  ExecuteCommandBuffer(false, true);
}

void VKGfx::BindBackbuffer(const ClearColor& clear_color)
{
  g_scheduler->Record([](CommandBufferManager* command_buffer_mgr) {
    command_buffer_mgr->GetStateTracker()->EndRenderPass();
  });
  if (!g_scheduler->CheckLastPresentDone())
    g_scheduler->SynchronizeSubmissionThread();

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

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkResult res;
  const bool present_fail = g_scheduler->CheckLastPresentFail();
  if (!present_fail)
  {
    semaphore = m_swap_chain->GetNextSemaphore();
    g_scheduler->Record([c_semaphore = semaphore](CommandBufferManager* command_buffer_mgr) {
      command_buffer_mgr->SetWaitSemaphoreForCurrentCommandBuffer(c_semaphore);
    });
    res = m_swap_chain->AcquireNextImage(semaphore);
  }
  else
  {
    res = g_scheduler->GetLastPresentResult();
  }

  if (res == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT &&
      !m_swap_chain->GetCurrentFullscreenState())
  {
    // AMD's binary driver as of 21.3 seems to return exclusive fullscreen lost even when it was
    // never requested, so long as the caller requested it to be application controlled. Handle
    // this ignoring the lost result and just continuing as normal if we never acquired it.
    res = VK_SUCCESS;
    if (present_fail)
    {
      if (semaphore == VK_NULL_HANDLE)
      {
        semaphore = m_swap_chain->GetNextSemaphore();
      }

      // We still need to acquire an image.
      res = m_swap_chain->AcquireNextImage(semaphore);
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

    semaphore = m_swap_chain->GetNextSemaphore();
    g_scheduler->Record([c_semaphore = semaphore](CommandBufferManager* command_buffer_mgr) {
      command_buffer_mgr->SetWaitSemaphoreForCurrentCommandBuffer(c_semaphore);
    });

    res = m_swap_chain->AcquireNextImage(semaphore);
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
      PanicAlertFmt("Failed to grab image from swap chain: {:#010X} {}", Common::ToUnderlying(res),
                    VkResultToString(res));
  }

  // Transition from undefined (or present src, but it can be substituted) to
  // color attachment ready for writing. These transitions must occur outside
  // a render pass, unless the render pass declares a self-dependency.
  m_swap_chain->GetCurrentTexture()->OverrideImageLayout(VK_IMAGE_LAYOUT_UNDEFINED);
  m_swap_chain->GetCurrentTexture()->TransitionToLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  SetAndClearFramebuffer(m_swap_chain->GetCurrentFramebuffer(),
                         ClearColor{{0.0f, 0.0f, 0.0f, 1.0f}});
}

void VKGfx::PresentBackbuffer()
{
  // End drawing to backbuffer
  g_scheduler->Record([](CommandBufferManager* command_buffer_mgr) {
    command_buffer_mgr->GetStateTracker()->EndRenderPass();
  });

  // Transition the backbuffer to PRESENT_SRC to ensure all commands drawing
  // to it have finished before present.
  m_swap_chain->GetCurrentTexture()->TransitionToLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // Submit the current command buffer, signaling rendering finished semaphore when it's done
  // Because this final command buffer is rendering to the swap chain, we need to wait for
  // the available semaphore to be signaled before executing the buffer. This final submission
  // can happen off-thread in the background while we're preparing the next frame.
  g_scheduler->SubmitCommandBuffer(true, false, m_swap_chain->GetSwapChain(),
                                   m_swap_chain->GetCurrentImageIndex());
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
  g_scheduler->SubmitCommandBuffer(submit_off_thread, wait_for_completion);
}

void VKGfx::CheckForSurfaceChange()
{
  if (!g_presenter->SurfaceChangedTestAndClear() || !m_swap_chain)
    return;

  g_scheduler->SyncWorker();

  // Submit the current draws up until rendering the XFB.
  ExecuteCommandBuffer(false, true);

  // Clear the present failed flag, since we don't want to resize after recreating.
  g_scheduler->CheckLastPresentFail();

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

  g_scheduler->SyncWorker();

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
  g_scheduler->CheckLastPresentFail();

  // Resize the swap chain.
  m_swap_chain->RecreateSwapChain();
  OnSwapChainResized();
}

void VKGfx::OnConfigChanged(u32 bits)
{
  AbstractGfx::OnConfigChanged(bits);

  if (bits & CONFIG_CHANGE_BIT_HOST_CONFIG)
  {
    g_scheduler->Record([](CommandBufferManager* command_buffer_manager) {
      g_object_cache->ReloadPipelineCache();
    });
  }

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
  g_scheduler->Record([](CommandBufferManager* command_buffer_mgr) {
    command_buffer_mgr->GetStateTracker()->EndRenderPass();
  });

  // Shouldn't be bound as a texture.
  fb->Unbind();

  fb->TransitionForRender();

  FramebufferState fb_state = {fb->GetFB(), fb->GetRect(), fb->GetLoadRenderPass(),
                               fb->GetDiscardRenderPass(), fb->GetClearRenderPass()};

  g_scheduler->Record([c_fb_state = fb_state](CommandBufferManager* command_buffer_mgr) {
    command_buffer_mgr->GetStateTracker()->SetFramebuffer(c_fb_state);
  });
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
  g_scheduler->Record([](CommandBufferManager* command_buffer_mgr) {
    command_buffer_mgr->GetStateTracker()->BeginDiscardRenderPass();
  });
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
      g_scheduler->Record([](CommandBufferManager* command_buffer_mgr) {
        if (command_buffer_mgr->GetStateTracker()->InRenderPass())
        {
          WARN_LOG_FMT(VIDEO, "Transitioning image in render pass in Renderer::SetTexture()");
          command_buffer_mgr->GetStateTracker()->EndRenderPass();
        }
      });

      tex->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    g_scheduler->Record([c_view = tex->GetView(), index](CommandBufferManager* command_buffer_mgr) {
      command_buffer_mgr->GetStateTracker()->SetTexture(index, c_view);
    });
  }
  else
  {
    g_scheduler->Record([](CommandBufferManager* command_buffer_mgr) {
      command_buffer_mgr->GetStateTracker()->SetTexture(0, VK_NULL_HANDLE);
    });
  }
}

void VKGfx::SetSamplerState(u32 index, const SamplerState& state)
{
  // Skip lookup if the state hasn't changed.
  if (m_sampler_states[index] == state)
    return;

  g_scheduler->Record([index, c_sampler_state = state](CommandBufferManager* command_buffer_mgr) {
    // Look up new state and replace in state tracker.
    VkSampler sampler = g_object_cache->GetSampler(c_sampler_state);
    if (sampler == VK_NULL_HANDLE)
    {
      ERROR_LOG_FMT(VIDEO, "Failed to create sampler");
      sampler = g_object_cache->GetPointSampler();
    }

    command_buffer_mgr->GetStateTracker()->SetSampler(index, sampler);
  });
  m_sampler_states[index] = state;
}

void VKGfx::SetComputeImageTexture(u32 index, AbstractTexture* texture, bool read, bool write)
{
  VKTexture* vk_texture = static_cast<VKTexture*>(texture);
  if (vk_texture)
  {
    g_scheduler->Record(
        [index, c_view = vk_texture->GetView()](CommandBufferManager* command_buffer_mgr) {
          command_buffer_mgr->GetStateTracker()->EndRenderPass();
          command_buffer_mgr->GetStateTracker()->SetImageTexture(index, c_view);
        });

    vk_texture->TransitionToLayout(read ? (write ? VKTexture::ComputeImageLayout::ReadWrite :
                                                   VKTexture::ComputeImageLayout::ReadOnly) :
                                          VKTexture::ComputeImageLayout::WriteOnly);
  }
  else
  {
    g_scheduler->Record([index](CommandBufferManager* command_buffer_mgr) {
      command_buffer_mgr->GetStateTracker()->SetImageTexture(index, VK_NULL_HANDLE);
    });
  }
}

void VKGfx::UnbindTexture(const AbstractTexture* texture)
{
  g_scheduler->Record([c_view = static_cast<const VKTexture*>(texture)->GetView()](
                          CommandBufferManager* command_buffer_mgr) {
    command_buffer_mgr->GetStateTracker()->UnbindTexture(c_view);
  });
}

void VKGfx::ResetSamplerStates()
{
  for (u32 i = 0; i < m_sampler_states.size(); i++)
  {
    m_sampler_states[i] = RenderState::GetPointSamplerState();
  }

  g_scheduler->Record(
      [c_sampler_count = m_sampler_states.size()](CommandBufferManager* command_buffer_mgr) {
        // Invalidate all sampler states, next draw will re-initialize them.
        for (u32 i = 0; i < c_sampler_count; i++)
        {
          command_buffer_mgr->GetStateTracker()->SetSampler(i, g_object_cache->GetPointSampler());
        }

        // Invalidate all sampler objects (some will be unused now).
        g_object_cache->ClearSamplerCache();
      });
}

void VKGfx::SetScissorRect(const MathUtil::Rectangle<int>& rc)
{
  g_scheduler->Record([c_rc = rc](CommandBufferManager* command_buffer_mgr) {
    VkRect2D scissor = {{c_rc.left, c_rc.top},
                        {static_cast<u32>(c_rc.GetWidth()), static_cast<u32>(c_rc.GetHeight())}};

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
    command_buffer_mgr->GetStateTracker()->SetScissor(scissor);
  });
}

void VKGfx::SetViewport(float x, float y, float width, float height, float near_depth,
                        float far_depth)
{
  VkViewport viewport = {x, y, width, height, near_depth, far_depth};
  g_scheduler->Record([viewport](CommandBufferManager* command_buffer_mgr) {
    command_buffer_mgr->GetStateTracker()->SetViewport(viewport);
  });
}

void VKGfx::Draw(u32 base_vertex, u32 num_vertices)
{
  g_scheduler->Record([base_vertex, num_vertices](CommandBufferManager* command_buffer_mgr) {
    if (!command_buffer_mgr->GetStateTracker()->Bind())
      return;

    vkCmdDraw(command_buffer_mgr->GetCurrentCommandBuffer(), num_vertices, 1, base_vertex, 0);
  });
}

void VKGfx::DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex)
{
  g_scheduler->Record(
      [base_vertex, num_indices, base_index](CommandBufferManager* command_buffer_mgr) {
        if (!command_buffer_mgr->GetStateTracker()->Bind())
          return;

        vkCmdDrawIndexed(command_buffer_mgr->GetCurrentCommandBuffer(), num_indices, 1, base_index,
                         base_vertex, 0);
      });
}

void VKGfx::DispatchComputeShader(const AbstractShader* shader, u32 groupsize_x, u32 groupsize_y,
                                  u32 groupsize_z, u32 groups_x, u32 groups_y, u32 groups_z)
{
  g_scheduler->Record([groups_x, groups_y, groups_z,
                       shader](CommandBufferManager* command_buffer_mgr) {
    if (!command_buffer_mgr->GetStateTracker()->Bind())
      return;

    command_buffer_mgr->GetStateTracker()->SetComputeShader(static_cast<const VKShader*>(shader));
    if (command_buffer_mgr->GetStateTracker()->BindCompute())
      vkCmdDispatch(command_buffer_mgr->GetCurrentCommandBuffer(), groups_x, groups_y, groups_z);
  });
}

SurfaceInfo VKGfx::GetSurfaceInfo() const
{
  return {m_swap_chain ? m_swap_chain->GetWidth() : 1u,
          m_swap_chain ? m_swap_chain->GetHeight() : 0u, m_backbuffer_scale,
          m_swap_chain ? m_swap_chain->GetTextureFormat() : AbstractTextureFormat::Undefined};
}

}  // namespace Vulkan
