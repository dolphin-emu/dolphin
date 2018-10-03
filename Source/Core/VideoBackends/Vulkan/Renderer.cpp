// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
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
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VKPipeline.h"
#include "VideoBackends/Vulkan/VKShader.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
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

  DestroyShaders();
  DestroySemaphores();
}

Renderer* Renderer::GetInstance()
{
  return static_cast<Renderer*>(g_renderer.get());
}

bool Renderer::IsHeadless() const
{
  return m_swap_chain == nullptr;
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

std::unique_ptr<AbstractTexture> Renderer::CreateTexture(const TextureConfig& config)
{
  return VKTexture::Create(config);
}

std::unique_ptr<AbstractStagingTexture> Renderer::CreateStagingTexture(StagingTextureType type,
                                                                       const TextureConfig& config)
{
  return VKStagingTexture::Create(type, config);
}

std::unique_ptr<AbstractShader> Renderer::CreateShaderFromSource(ShaderStage stage,
                                                                 const char* source, size_t length)
{
  return VKShader::CreateFromSource(stage, source, length);
}

std::unique_ptr<AbstractShader> Renderer::CreateShaderFromBinary(ShaderStage stage,
                                                                 const void* data, size_t length)
{
  return VKShader::CreateFromBinary(stage, data, length);
}

std::unique_ptr<AbstractPipeline> Renderer::CreatePipeline(const AbstractPipelineConfig& config)
{
  return VKPipeline::Create(config);
}

std::unique_ptr<AbstractFramebuffer>
Renderer::CreateFramebuffer(const AbstractTexture* color_attachment,
                            const AbstractTexture* depth_attachment)
{
  return VKFramebuffer::Create(static_cast<const VKTexture*>(color_attachment),
                               static_cast<const VKTexture*>(depth_attachment));
}

std::tuple<VkBuffer, u32> Renderer::UpdateUtilityUniformBuffer(const void* uniforms,
                                                               u32 uniforms_size)
{
  StreamBuffer* ubo_buf = g_object_cache->GetUtilityShaderUniformBuffer();
  if (!ubo_buf->ReserveMemory(uniforms_size, g_vulkan_context->GetUniformBufferAlignment()))
  {
    Util::ExecuteCurrentCommandsAndRestoreState(false, true);
    if (!ubo_buf->ReserveMemory(uniforms_size, g_vulkan_context->GetUniformBufferAlignment()))
    {
      PanicAlert("Failed to reserve uniform buffer space for utility draw.");
      return {};
    }
  }

  VkBuffer ubo = ubo_buf->GetBuffer();
  u32 ubo_offset = static_cast<u32>(ubo_buf->GetCurrentOffset());
  std::memcpy(ubo_buf->GetCurrentHostPointer(), uniforms, uniforms_size);
  ubo_buf->CommitMemory(uniforms_size);

  return std::tie(ubo, ubo_offset);
}

void Renderer::SetPipeline(const AbstractPipeline* pipeline)
{
  StateTracker::GetInstance()->SetPipeline(static_cast<const VKPipeline*>(pipeline));
}

void Renderer::DrawUtilityPipeline(const void* uniforms, u32 uniforms_size, const void* vertices,
                                   u32 vertex_stride, u32 num_vertices)
{
  // Binding the utility pipeline layout breaks the standard layout.
  StateTracker::GetInstance()->SetPendingRebind();

  // Upload uniforms.
  VkBuffer uniform_buffer = g_object_cache->GetUtilityShaderUniformBuffer()->GetBuffer();
  u32 uniform_buffer_offset = 0;
  if (uniforms_size > 0)
    std::tie(uniform_buffer, uniform_buffer_offset) =
        UpdateUtilityUniformBuffer(uniforms, uniforms_size);

  // Upload vertices.
  VkBuffer vertex_buffer = VK_NULL_HANDLE;
  VkDeviceSize vertex_buffer_offset = 0;
  if (vertices)
  {
    u32 vertices_size = vertex_stride * num_vertices;
    StreamBuffer* vbo_buf = g_object_cache->GetUtilityShaderVertexBuffer();
    if (!vbo_buf->ReserveMemory(vertices_size, vertex_stride))
    {
      Util::ExecuteCurrentCommandsAndRestoreState(true);
      if (!vbo_buf->ReserveMemory(vertices_size, vertex_stride))
      {
        PanicAlert("Failed to reserve vertex buffer space for utility draw.");
        return;
      }
    }

    vertex_buffer = vbo_buf->GetBuffer();
    vertex_buffer_offset = vbo_buf->GetCurrentOffset();
    std::memcpy(vbo_buf->GetCurrentHostPointer(), vertices, vertices_size);
    vbo_buf->CommitMemory(vertices_size);
  }

  // Allocate descriptor sets.
  std::array<VkDescriptorSet, 2> dsets;
  dsets[0] = g_command_buffer_mgr->AllocateDescriptorSet(
      g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_SINGLE_UNIFORM_BUFFER));
  dsets[1] = g_command_buffer_mgr->AllocateDescriptorSet(
      g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_PIXEL_SHADER_SAMPLERS));

  // Flush first if failed.
  if (dsets[0] == VK_NULL_HANDLE || dsets[1] == VK_NULL_HANDLE)
  {
    Util::ExecuteCurrentCommandsAndRestoreState(true);
    dsets[0] = g_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_SINGLE_UNIFORM_BUFFER));
    dsets[1] = g_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_PIXEL_SHADER_SAMPLERS));

    if (dsets[0] == VK_NULL_HANDLE || dsets[1] == VK_NULL_HANDLE)
    {
      PanicAlert("Failed to allocate descriptor sets in utility draw.");
      return;
    }
  }

  // Build UBO descriptor set.
  std::array<VkWriteDescriptorSet, 2> dswrites;
  VkDescriptorBufferInfo dsbuffer = {uniform_buffer, 0, std::max(uniforms_size, 4u)};
  dswrites[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    nullptr, dsets[0],  0,      0, 1,
                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr, &dsbuffer, nullptr};
  dswrites[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 nullptr,
                 dsets[1],
                 0,
                 0,
                 NUM_PIXEL_SHADER_SAMPLERS,
                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 StateTracker::GetInstance()->GetPSSamplerBindings().data(),
                 nullptr,
                 nullptr};

  // Build commands.
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    StateTracker::GetInstance()->GetPipeline()->GetVkPipeline());
  if (vertex_buffer != VK_NULL_HANDLE)
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &vertex_buffer_offset);

  // Update and bind descriptors.
  VkPipelineLayout pipeline_layout = g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_UTILITY);
  vkUpdateDescriptorSets(g_vulkan_context->GetDevice(), static_cast<u32>(dswrites.size()),
                         dswrites.data(), 0, nullptr);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0,
                          static_cast<u32>(dsets.size()), dsets.data(), 1, &uniform_buffer_offset);

  // Ensure we're in a render pass before drawing, just in case we had to flush.
  StateTracker::GetInstance()->BeginRenderPass();
  vkCmdDraw(command_buffer, num_vertices, 1, 0, 0);
}

void Renderer::DispatchComputeShader(const AbstractShader* shader, const void* uniforms,
                                     u32 uniforms_size, u32 groups_x, u32 groups_y, u32 groups_z)
{
  // Binding the utility pipeline layout breaks the standard layout.
  StateTracker::GetInstance()->SetPendingRebind();
  StateTracker::GetInstance()->EndRenderPass();

  // Upload uniforms.
  VkBuffer uniform_buffer = g_object_cache->GetUtilityShaderUniformBuffer()->GetBuffer();
  u32 uniform_buffer_offset = 0;
  if (uniforms_size > 0)
    std::tie(uniform_buffer, uniform_buffer_offset) =
        UpdateUtilityUniformBuffer(uniforms, uniforms_size);

  // Flush first if failed.
  VkDescriptorSet dset = g_command_buffer_mgr->AllocateDescriptorSet(
      g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_COMPUTE));
  if (dset == VK_NULL_HANDLE)
  {
    Util::ExecuteCurrentCommandsAndRestoreState(true);
    dset = g_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_COMPUTE));
    if (dset == VK_NULL_HANDLE)
    {
      PanicAlert("Failed to allocate descriptor sets in utility dispatch.");
      return;
    }
  }

  std::array<VkWriteDescriptorSet, 2> dswrites;
  VkDescriptorBufferInfo dsbuffer = {uniform_buffer, 0, std::max(uniforms_size, 4u)};
  dswrites[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    nullptr, dset,      0,      0, 1,
                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr, &dsbuffer, nullptr};
  dswrites[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 nullptr,
                 dset,
                 1,
                 0,
                 NUM_PIXEL_SHADER_SAMPLERS,
                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 StateTracker::GetInstance()->GetPSSamplerBindings().data(),
                 nullptr,
                 nullptr};

  // TODO: Texel buffers, storage images.

  // Build commands.
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
  VkPipelineLayout pipeline_layout = g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_UTILITY);
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    static_cast<const VKShader*>(shader)->GetComputePipeline());
  vkUpdateDescriptorSets(g_vulkan_context->GetDevice(), static_cast<u32>(dswrites.size()),
                         dswrites.data(), 0, nullptr);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1,
                          &dset, 1, &uniform_buffer_offset);
  vkCmdDispatch(command_buffer, groups_x, groups_y, groups_z);
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
    const std::array<VkClearValue, 2> clear_values = {{clear_color_value, clear_depth_value}};
    StateTracker::GetInstance()->BeginClearRenderPass(target_vk_rc, clear_values.data(),
                                                      static_cast<u32>(clear_values.size()));
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

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->SetPendingRebind();
  FramebufferManager::GetInstance()->ReinterpretPixelData(convtype);

  // EFB framebuffer has now changed, so update accordingly.
  BindEFBToStateTracker();
}

void Renderer::SwapImpl(AbstractTexture* texture, const EFBRectangle& xfb_region, u64 ticks)
{
  // Pending/batched EFB pokes should be included in the final image.
  FramebufferManager::GetInstance()->FlushEFBPokes();

  auto* xfb_texture = static_cast<VKTexture*>(texture);

  // End the current render pass.
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->OnEndFrame();

  // Handle host window resizes.
  CheckForSurfaceChange();
  CheckForSurfaceResize();

  // There are a few variables which can alter the final window draw rectangle, and some of them
  // are determined by guest state. Currently, the only way to catch these is to update every frame.
  UpdateDrawRectangle();

  // Ensure the worker thread is not still submitting a previous command buffer.
  // In other words, the last frame has been submitted (otherwise the next call would
  // be a race, as the image may not have been consumed yet).
  g_command_buffer_mgr->PrepareToSubmitCommandBuffer();

  // Draw to the screen if we have a swap chain.
  if (m_swap_chain)
  {
    DrawScreen(xfb_texture, xfb_region);

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
  RestoreAPIState();

  // Determine what (if anything) has changed in the config.
  CheckForConfigChanges();

  // Clean up stale textures.
  TextureCache::GetInstance()->Cleanup(frameCount);
}

void Renderer::DrawScreen(VKTexture* xfb_texture, const EFBRectangle& xfb_region)
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
  m_current_framebuffer = nullptr;
  m_current_framebuffer_width = backbuffer->GetWidth();
  m_current_framebuffer_height = backbuffer->GetHeight();

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

  // Draw
  BlitScreen(m_swap_chain->GetRenderPass(), GetTargetRectangle(), xfb_region,
             xfb_texture->GetRawTexIdentifier());

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

void Renderer::BlitScreen(VkRenderPass render_pass, const TargetRectangle& dst_rect,
                          const TargetRectangle& src_rect, const Texture2D* src_tex)
{
  VulkanPostProcessing* post_processor = static_cast<VulkanPostProcessing*>(m_post_processor.get());
  if (g_ActiveConfig.stereo_mode == StereoMode::SBS ||
      g_ActiveConfig.stereo_mode == StereoMode::TAB)
  {
    TargetRectangle left_rect;
    TargetRectangle right_rect;
    std::tie(left_rect, right_rect) = ConvertStereoRectangle(dst_rect);

    post_processor->BlitFromTexture(left_rect, src_rect, src_tex, 0, render_pass);
    post_processor->BlitFromTexture(right_rect, src_rect, src_tex, 1, render_pass);
  }
  else if (g_ActiveConfig.stereo_mode == StereoMode::QuadBuffer)
  {
    post_processor->BlitFromTexture(dst_rect, src_rect, src_tex, -1, render_pass);
  }
  else
  {
    post_processor->BlitFromTexture(dst_rect, src_rect, src_tex, 0, render_pass);
  }
}

void Renderer::CheckForSurfaceChange()
{
  if (!m_surface_changed.TestAndClear() || !m_swap_chain)
    return;

  // Submit the current draws up until rendering the XFB.
  g_command_buffer_mgr->ExecuteCommandBuffer(false, false);
  g_command_buffer_mgr->WaitForGPUIdle();

  // Clear the present failed flag, since we don't want to resize after recreating.
  g_command_buffer_mgr->CheckLastPresentFail();

  // Recreate the surface. If this fails we're in trouble.
  if (!m_swap_chain->RecreateSurface(m_new_surface_handle))
    PanicAlert("Failed to recreate Vulkan surface. Cannot continue.");
  m_new_surface_handle = nullptr;

  // Handle case where the dimensions are now different.
  OnSwapChainResized();
}

void Renderer::CheckForSurfaceResize()
{
  if (!m_surface_resized.TestAndClear())
    return;

  // If we don't have a surface, how can we resize the swap chain?
  // CheckForSurfaceChange should handle this case.
  if (!m_swap_chain)
  {
    WARN_LOG(VIDEO, "Surface resize event received without active surface, ignoring");
    return;
  }

  // Wait for the GPU to catch up since we're going to destroy the swap chain.
  g_command_buffer_mgr->ExecuteCommandBuffer(false, false);
  g_command_buffer_mgr->WaitForGPUIdle();

  // Clear the present failed flag, since we don't want to resize after recreating.
  g_command_buffer_mgr->CheckLastPresentFail();

  // Resize the swap chain.
  m_swap_chain->RecreateSwapChain();
  OnSwapChainResized();
}

void Renderer::CheckForConfigChanges()
{
  // Save the video config so we can compare against to determine which settings have changed.
  const u32 old_multisamples = g_ActiveConfig.iMultisamples;
  const int old_anisotropy = g_ActiveConfig.iMaxAnisotropy;
  const bool old_force_filtering = g_ActiveConfig.bForceFiltering;

  // Copy g_Config to g_ActiveConfig.
  // NOTE: This can potentially race with the UI thread, however if it does, the changes will be
  // delayed until the next time CheckForConfigChanges is called.
  UpdateActiveConfig();

  // Determine which (if any) settings have changed.
  const bool multisamples_changed = old_multisamples != g_ActiveConfig.iMultisamples;
  const bool anisotropy_changed = old_anisotropy != g_ActiveConfig.iMaxAnisotropy;
  const bool force_texture_filtering_changed =
      old_force_filtering != g_ActiveConfig.bForceFiltering;

  // Update texture cache settings with any changed options.
  TextureCache::GetInstance()->OnConfigChanged(g_ActiveConfig);

  // Handle settings that can cause the EFB framebuffer to change.
  if (CalculateTargetSize() || multisamples_changed)
    RecreateEFBFramebuffer();

  // MSAA samples changed, we need to recreate the EFB render pass.
  // If the stereoscopy mode changed, we need to recreate the buffers as well.
  // SSAA changed on/off, we have to recompile shaders.
  // Changing stereoscopy from off<->on also requires shaders to be recompiled.
  if (CheckForHostConfigChanges())
  {
    RecreateEFBFramebuffer();
    RecompileShaders();
    FramebufferManager::GetInstance()->RecompileShaders();
    g_shader_cache->ReloadPipelineCache();
    g_shader_cache->RecompileSharedShaders();
  }

  // For vsync, we need to change the present mode, which means recreating the swap chain.
  if (m_swap_chain && g_ActiveConfig.IsVSync() != m_swap_chain->IsVSyncEnabled())
  {
    g_command_buffer_mgr->WaitForGPUIdle();
    m_swap_chain->SetVSync(g_ActiveConfig.IsVSync());
  }

  // For quad-buffered stereo we need to change the layer count, so recreate the swap chain.
  if (m_swap_chain &&
      (g_ActiveConfig.stereo_mode == StereoMode::QuadBuffer) != m_swap_chain->IsStereoEnabled())
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
  m_current_framebuffer = nullptr;
  m_current_framebuffer_width = FramebufferManager::GetInstance()->GetEFBWidth();
  m_current_framebuffer_height = FramebufferManager::GetInstance()->GetEFBHeight();
}

void Renderer::RecreateEFBFramebuffer()
{
  // Ensure the GPU is finished with the current EFB textures.
  g_command_buffer_mgr->WaitForGPUIdle();
  FramebufferManager::GetInstance()->RecreateEFBFramebuffer();
  BindEFBToStateTracker();

  // Viewport and scissor rect have to be reset since they will be scaled differently.
  BPFunctions::SetViewport();
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
  StateTracker::GetInstance()->EndRenderPass();
  if (m_current_framebuffer)
    static_cast<const VKFramebuffer*>(m_current_framebuffer)->TransitionForSample();

  BindEFBToStateTracker();

  // Instruct the state tracker to re-bind everything before the next draw
  StateTracker::GetInstance()->SetPendingRebind();
}

void Renderer::BindFramebuffer(const VKFramebuffer* fb)
{
  const VkRect2D render_area = {static_cast<int>(fb->GetWidth()),
                                static_cast<int>(fb->GetHeight())};

  StateTracker::GetInstance()->EndRenderPass();
  if (m_current_framebuffer)
    static_cast<const VKFramebuffer*>(m_current_framebuffer)->TransitionForSample();

  fb->TransitionForRender();
  StateTracker::GetInstance()->SetFramebuffer(fb->GetFB(), render_area);
  StateTracker::GetInstance()->SetRenderPass(fb->GetLoadRenderPass(), fb->GetClearRenderPass());
  m_current_framebuffer = fb;
  m_current_framebuffer_width = fb->GetWidth();
  m_current_framebuffer_height = fb->GetHeight();
}

void Renderer::SetFramebuffer(const AbstractFramebuffer* framebuffer)
{
  const VKFramebuffer* vkfb = static_cast<const VKFramebuffer*>(framebuffer);
  BindFramebuffer(vkfb);
  StateTracker::GetInstance()->BeginRenderPass();
}

void Renderer::SetAndDiscardFramebuffer(const AbstractFramebuffer* framebuffer)
{
  const VKFramebuffer* vkfb = static_cast<const VKFramebuffer*>(framebuffer);
  BindFramebuffer(vkfb);

  // If we're discarding, begin the discard pass, then switch to a load pass.
  // This way if the command buffer is flushed, we don't start another discard pass.
  StateTracker::GetInstance()->SetRenderPass(vkfb->GetDiscardRenderPass(),
                                             vkfb->GetClearRenderPass());
  StateTracker::GetInstance()->BeginRenderPass();
  StateTracker::GetInstance()->SetRenderPass(vkfb->GetLoadRenderPass(), vkfb->GetClearRenderPass());
}

void Renderer::SetAndClearFramebuffer(const AbstractFramebuffer* framebuffer,
                                      const ClearColor& color_value, float depth_value)
{
  const VKFramebuffer* vkfb = static_cast<const VKFramebuffer*>(framebuffer);
  BindFramebuffer(vkfb);

  const VkRect2D render_area = {static_cast<int>(vkfb->GetWidth()),
                                static_cast<int>(vkfb->GetHeight())};
  std::array<VkClearValue, 2> clear_values;
  u32 num_clear_values = 0;
  if (vkfb->GetColorFormat() != AbstractTextureFormat::Undefined)
  {
    std::memcpy(clear_values[num_clear_values].color.float32, color_value.data(),
                sizeof(clear_values[num_clear_values].color.float32));
    num_clear_values++;
  }
  if (vkfb->GetDepthFormat() != AbstractTextureFormat::Undefined)
  {
    clear_values[num_clear_values].depthStencil.depth = depth_value;
    clear_values[num_clear_values].depthStencil.stencil = 0;
    num_clear_values++;
  }
  StateTracker::GetInstance()->BeginClearRenderPass(render_area, clear_values.data(),
                                                    num_clear_values);
}

void Renderer::SetTexture(u32 index, const AbstractTexture* texture)
{
  // Texture should always be in SHADER_READ_ONLY layout prior to use.
  // This is so we don't need to transition during render passes.
  auto* tex = texture ? static_cast<const VKTexture*>(texture)->GetRawTexIdentifier() : nullptr;
  DEBUG_ASSERT(!tex || tex->GetLayout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  StateTracker::GetInstance()->SetTexture(index, tex ? tex->GetView() : VK_NULL_HANDLE);
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

void Renderer::UnbindTexture(const AbstractTexture* texture)
{
  StateTracker::GetInstance()->UnbindTexture(
      static_cast<const VKTexture*>(texture)->GetRawTexIdentifier()->GetView());
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

void Renderer::SetScissorRect(const MathUtil::Rectangle<int>& rc)
{
  VkRect2D scissor = {{rc.left, rc.top},
                      {static_cast<u32>(rc.GetWidth()), static_cast<u32>(rc.GetHeight())}};
  StateTracker::GetInstance()->SetScissor(scissor);
}

void Renderer::SetViewport(float x, float y, float width, float height, float near_depth,
                           float far_depth)
{
  VkViewport viewport = {x,          y,        std::max(width, 1.0f), std::max(height, 1.0f),
                         near_depth, far_depth};
  StateTracker::GetInstance()->SetViewport(viewport);
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
