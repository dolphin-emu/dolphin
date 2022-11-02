// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/StateTracker.h"

#include "Common/Assert.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/VKGfx.h"
#include "VideoBackends/Vulkan/VKPipeline.h"
#include "VideoBackends/Vulkan/VKShader.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VKVertexFormat.h"
#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/Constants.h"

namespace Vulkan
{
StateTracker::StateTracker(CommandBufferManager* command_buffer_mgr)
    : m_command_buffer_mgr(command_buffer_mgr)
{
}

StateTracker::~StateTracker()
{
  vkDestroyImageView(g_vulkan_context->GetDevice(), m_dummy_view, nullptr);
  vmaDestroyImage(g_vulkan_context->GetMemoryAllocator(), m_dummy_image, m_dummy_image_alloc);
}

bool StateTracker::Initialize()
{
  // Create a dummy texture which can be used in place of a real binding.
  VkImageCreateInfo dummy_info;
  dummy_info.pNext = nullptr;
  dummy_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  dummy_info.extent = {1, 1, 1};
  dummy_info.arrayLayers = 1;
  dummy_info.mipLevels = 1;
  dummy_info.flags = 0;
  dummy_info.imageType = VK_IMAGE_TYPE_2D;
  dummy_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  dummy_info.samples = VK_SAMPLE_COUNT_1_BIT;
  dummy_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  dummy_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  dummy_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  dummy_info.queueFamilyIndexCount = 0;
  dummy_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
  dummy_info.pQueueFamilyIndices = nullptr;

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
  alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  alloc_create_info.pool = VK_NULL_HANDLE;
  alloc_create_info.pUserData = nullptr;
  alloc_create_info.priority = 0.0;
  alloc_create_info.requiredFlags = 0;
  alloc_create_info.preferredFlags = 0;

  VkResult res = vmaCreateImage(g_vulkan_context->GetMemoryAllocator(), &dummy_info,
                                &alloc_create_info, &m_dummy_image, &m_dummy_image_alloc, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vmaCreateImage failed: ");
    return false;
  }

  VkImageViewCreateInfo dummy_view_info;
  dummy_view_info.pNext = nullptr;
  dummy_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  dummy_view_info.flags = 0;
  dummy_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  dummy_view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  dummy_view_info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
  dummy_view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  dummy_view_info.image = m_dummy_image;

  res = vkCreateImageView(g_vulkan_context->GetDevice(), &dummy_view_info, nullptr, &m_dummy_view);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
    return false;
  }

  VkImageMemoryBarrier img_barrier;
  img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  img_barrier.pNext = nullptr;
  img_barrier.srcAccessMask = 0;
  img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  img_barrier.image = m_dummy_image;
  img_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  img_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  img_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  vkCmdPipelineBarrier(m_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &img_barrier);

  // Initialize all samplers to point by default
  for (size_t i = 0; i < VideoCommon::MAX_PIXEL_SHADER_SAMPLERS; i++)
  {
    m_bindings.samplers[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    m_bindings.samplers[i].imageView = m_dummy_view;
    m_bindings.samplers[i].sampler = g_object_cache->GetPointSampler();
  }

  for (size_t i = 0; i < VideoCommon::MAX_COMPUTE_SHADER_SAMPLERS; i++)
  {
    m_bindings.image_textures[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    m_bindings.image_textures[i].imageView = m_dummy_view;
    m_bindings.image_textures[i].sampler = g_object_cache->GetPointSampler();
  }

  // Default dirty flags include all descriptors
  InvalidateCachedState();
  return true;
}

void StateTracker::SetVertexBuffer(VkBuffer buffer, VkDeviceSize offset, u32 size)
{
  if (m_vertex_buffer != buffer || m_vertex_buffer_offset != offset)
  {
    m_vertex_buffer = buffer;
    m_vertex_buffer_offset = offset;
    m_dirty_flags |= DIRTY_FLAG_VERTEX_BUFFER;
  }
  if (m_bindings.gx_uber_vertex_ssbo.buffer != buffer ||
      m_bindings.gx_uber_vertex_ssbo.offset != offset ||
      m_bindings.gx_uber_vertex_ssbo.range != size)
  {
    m_bindings.gx_uber_vertex_ssbo.buffer = buffer;
    m_bindings.gx_uber_vertex_ssbo.offset = offset;
    m_bindings.gx_uber_vertex_ssbo.range = size;
    m_dirty_flags |= DIRTY_FLAG_GX_SSBO;
  }
}

void StateTracker::SetIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType type)
{
  if (m_index_buffer == buffer && m_index_buffer_offset == offset && m_index_type == type)
    return;

  m_index_buffer = buffer;
  m_index_buffer_offset = offset;
  m_index_type = type;
  m_dirty_flags |= DIRTY_FLAG_INDEX_BUFFER;
}

void StateTracker::SetFramebuffer(FramebufferState framebuffer_state)
{
  // Should not be changed within a render pass.
  ASSERT(!InRenderPass());
  m_framebuffer_state = framebuffer_state;
}

void StateTracker::SetPipeline(const VKPipeline* pipeline)
{
  if (m_pipeline == pipeline)
    return;

  // If the usage changes, we need to re-bind everything, as the layout is different.
  const bool new_usage =
      pipeline && (!m_pipeline || m_pipeline->GetUsage() != pipeline->GetUsage());

  m_pipeline = pipeline;
  m_dirty_flags |= DIRTY_FLAG_PIPELINE;
  if (new_usage)
    m_dirty_flags |= DIRTY_FLAG_DESCRIPTOR_SETS;
}

void StateTracker::SetComputeShader(const VKShader* shader)
{
  if (m_compute_shader == shader)
    return;

  m_compute_shader = shader;
  m_dirty_flags |= DIRTY_FLAG_COMPUTE_SHADER;
}

void StateTracker::SetGXUniformBuffer(u32 index, VkBuffer buffer, u32 offset, u32 size)
{
  auto& binding = m_bindings.gx_ubo_bindings[index];
  if (binding.buffer != buffer || binding.range != size)
  {
    binding.buffer = buffer;
    binding.range = size;
    m_dirty_flags |= DIRTY_FLAG_GX_UBOS;
  }

  if (m_bindings.gx_ubo_offsets[index] != offset)
  {
    m_bindings.gx_ubo_offsets[index] = offset;
    m_dirty_flags |= DIRTY_FLAG_GX_UBO_OFFSETS;
  }
}

void StateTracker::SetUtilityUniformBuffer(VkBuffer buffer, u32 offset, u32 size)
{
  auto& binding = m_bindings.utility_ubo_binding;
  if (binding.buffer != buffer || binding.range != size)
  {
    binding.buffer = buffer;
    binding.range = size;
    m_dirty_flags |= DIRTY_FLAG_UTILITY_UBO;
  }

  if (m_bindings.utility_ubo_offset != offset)
  {
    m_bindings.utility_ubo_offset = offset;
    m_dirty_flags |= DIRTY_FLAG_UTILITY_UBO_OFFSET | DIRTY_FLAG_COMPUTE_DESCRIPTOR_SET;
  }
}

void StateTracker::SetTexture(u32 index, VkImageView view)
{
  if (m_bindings.samplers[index].imageView == view)
    return;

  m_bindings.samplers[index].imageView = view;
  m_bindings.samplers[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  m_dirty_flags |=
      DIRTY_FLAG_GX_SAMPLERS | DIRTY_FLAG_UTILITY_BINDINGS | DIRTY_FLAG_COMPUTE_BINDINGS;
}

void StateTracker::SetSampler(u32 index, VkSampler sampler)
{
  if (m_bindings.samplers[index].sampler == sampler)
    return;

  m_bindings.samplers[index].sampler = sampler;
  m_dirty_flags |=
      DIRTY_FLAG_GX_SAMPLERS | DIRTY_FLAG_UTILITY_BINDINGS | DIRTY_FLAG_COMPUTE_BINDINGS;
}

void StateTracker::SetSSBO(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
  if (m_bindings.ssbo.buffer == buffer && m_bindings.ssbo.offset == offset &&
      m_bindings.ssbo.range == range)
  {
    return;
  }

  m_bindings.ssbo.buffer = buffer;
  m_bindings.ssbo.offset = offset;
  m_bindings.ssbo.range = range;
  m_dirty_flags |= DIRTY_FLAG_GX_SSBO;
}

void StateTracker::SetTexelBuffer(u32 index, VkBufferView view)
{
  if (m_bindings.texel_buffers[index] == view)
    return;

  m_bindings.texel_buffers[index] = view;
  m_dirty_flags |= DIRTY_FLAG_UTILITY_BINDINGS | DIRTY_FLAG_COMPUTE_BINDINGS;
}

void StateTracker::SetImageTexture(u32 index, VkImageView view)
{
  if (m_bindings.image_textures[index].imageView == view)
    return;

  m_bindings.image_textures[index].imageView = view;
  m_bindings.image_textures[index].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  m_dirty_flags |= DIRTY_FLAG_COMPUTE_BINDINGS;
}

void StateTracker::UnbindTexture(VkImageView view)
{
  for (VkDescriptorImageInfo& it : m_bindings.samplers)
  {
    if (it.imageView == view)
    {
      it.imageView = m_dummy_view;
      it.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
  }

  for (VkDescriptorImageInfo& it : m_bindings.image_textures)
  {
    if (it.imageView == view)
    {
      it.imageView = m_dummy_view;
      it.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
  }
}

void StateTracker::InvalidateCachedState()
{
  m_gx_descriptor_sets.fill(VK_NULL_HANDLE);
  m_utility_descriptor_sets.fill(VK_NULL_HANDLE);
  m_compute_descriptor_set = VK_NULL_HANDLE;
  m_dirty_flags |= DIRTY_FLAG_ALL_DESCRIPTORS | DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR |
                   DIRTY_FLAG_PIPELINE | DIRTY_FLAG_COMPUTE_SHADER | DIRTY_FLAG_DESCRIPTOR_SETS |
                   DIRTY_FLAG_COMPUTE_DESCRIPTOR_SET;
  if (m_vertex_buffer != VK_NULL_HANDLE)
    m_dirty_flags |= DIRTY_FLAG_VERTEX_BUFFER;
  if (m_index_buffer != VK_NULL_HANDLE)
    m_dirty_flags |= DIRTY_FLAG_INDEX_BUFFER;
}

void StateTracker::BeginRenderPass()
{
  if (InRenderPass())
    return;

  m_current_render_pass = m_framebuffer_state.load_render_pass;
  m_render_area = m_framebuffer_state.render_area;

  VkRenderPassBeginInfo begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                      nullptr,
                                      m_current_render_pass,
                                      m_framebuffer_state.framebuffer,
                                      m_render_area,
                                      0,
                                      nullptr};

  vkCmdBeginRenderPass(m_command_buffer_mgr->GetCurrentCommandBuffer(), &begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);
}

void StateTracker::BeginDiscardRenderPass()
{
  if (InRenderPass())
    return;

  m_current_render_pass = m_framebuffer_state.discard_render_pass;
  m_render_area = m_framebuffer_state.render_area;

  VkRenderPassBeginInfo begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                      nullptr,
                                      m_current_render_pass,
                                      m_framebuffer_state.framebuffer,
                                      m_render_area,
                                      0,
                                      nullptr};

  vkCmdBeginRenderPass(m_command_buffer_mgr->GetCurrentCommandBuffer(), &begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);
}

void StateTracker::EndRenderPass()
{
  if (!InRenderPass())
    return;

  vkCmdEndRenderPass(m_command_buffer_mgr->GetCurrentCommandBuffer());
  m_current_render_pass = VK_NULL_HANDLE;
}

void StateTracker::BeginClearRenderPass(const VkRect2D& area, const VkClearValue* clear_values,
                                        u32 num_clear_values)
{
  ASSERT(!InRenderPass());

  m_current_render_pass = m_framebuffer_state.clear_render_pass;
  m_render_area = area;

  VkRenderPassBeginInfo begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                      nullptr,
                                      m_current_render_pass,
                                      m_framebuffer_state.framebuffer,
                                      m_render_area,
                                      num_clear_values,
                                      clear_values};

  vkCmdBeginRenderPass(m_command_buffer_mgr->GetCurrentCommandBuffer(), &begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);
}

void StateTracker::SetViewport(const VkViewport& viewport)
{
  if (memcmp(&m_viewport, &viewport, sizeof(viewport)) == 0)
    return;

  m_viewport = viewport;
  m_dirty_flags |= DIRTY_FLAG_VIEWPORT;
}

void StateTracker::SetScissor(const VkRect2D& scissor)
{
  if (memcmp(&m_scissor, &scissor, sizeof(scissor)) == 0)
    return;

  m_scissor = scissor;
  m_dirty_flags |= DIRTY_FLAG_SCISSOR;
}

bool StateTracker::Bind()
{
  // Must have a pipeline.
  if (!m_pipeline)
    return false;

  // Check the render area if we were in a clear pass.
  if (m_current_render_pass == m_framebuffer_state.clear_render_pass &&
      !IsViewportWithinRenderArea())
    EndRenderPass();

  // Get a new descriptor set if any parts have changed
  UpdateDescriptorSet();

  // Start render pass if not already started
  if (!InRenderPass())
    BeginRenderPass();

  // Re-bind parts of the pipeline
  const VkCommandBuffer command_buffer = m_command_buffer_mgr->GetCurrentCommandBuffer();
  const bool needs_vertex_buffer = !g_ActiveConfig.backend_info.bSupportsDynamicVertexLoader ||
                                   m_pipeline->GetUsage() != AbstractPipelineUsage::GXUber;
  if (needs_vertex_buffer && (m_dirty_flags & DIRTY_FLAG_VERTEX_BUFFER))
  {
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &m_vertex_buffer, &m_vertex_buffer_offset);
    m_dirty_flags &= ~DIRTY_FLAG_VERTEX_BUFFER;
  }

  if (m_dirty_flags & DIRTY_FLAG_INDEX_BUFFER)
    vkCmdBindIndexBuffer(command_buffer, m_index_buffer, m_index_buffer_offset, m_index_type);

  if (m_dirty_flags & DIRTY_FLAG_PIPELINE)
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetVkPipeline());

  if (m_dirty_flags & DIRTY_FLAG_VIEWPORT)
    vkCmdSetViewport(command_buffer, 0, 1, &m_viewport);

  if (m_dirty_flags & DIRTY_FLAG_SCISSOR)
    vkCmdSetScissor(command_buffer, 0, 1, &m_scissor);

  m_dirty_flags &=
      ~(DIRTY_FLAG_INDEX_BUFFER | DIRTY_FLAG_PIPELINE | DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR);
  return true;
}

bool StateTracker::BindCompute()
{
  if (!m_compute_shader)
    return false;

  // Can't kick compute in a render pass.
  if (InRenderPass())
    EndRenderPass();

  const VkCommandBuffer command_buffer = m_command_buffer_mgr->GetCurrentCommandBuffer();
  if (m_dirty_flags & DIRTY_FLAG_COMPUTE_SHADER)
  {
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_compute_shader->GetComputePipeline());
  }

  UpdateComputeDescriptorSet();
  m_dirty_flags &= ~DIRTY_FLAG_COMPUTE_SHADER;
  return true;
}

bool StateTracker::IsWithinRenderArea(s32 x, s32 y, u32 width, u32 height) const
{
  // Check that the viewport does not lie outside the render area.
  // If it does, we need to switch to a normal load/store render pass.
  s32 left = m_render_area.offset.x;
  s32 top = m_render_area.offset.y;
  s32 right = left + static_cast<s32>(m_render_area.extent.width);
  s32 bottom = top + static_cast<s32>(m_render_area.extent.height);
  s32 test_left = x;
  s32 test_top = y;
  s32 test_right = test_left + static_cast<s32>(width);
  s32 test_bottom = test_top + static_cast<s32>(height);
  return test_left >= left && test_right <= right && test_top >= top && test_bottom <= bottom;
}

bool StateTracker::IsViewportWithinRenderArea() const
{
  return IsWithinRenderArea(static_cast<s32>(m_viewport.x), static_cast<s32>(m_viewport.y),
                            static_cast<u32>(m_viewport.width),
                            static_cast<u32>(m_viewport.height));
}

void StateTracker::EndClearRenderPass()
{
  if (m_current_render_pass != m_framebuffer_state.clear_render_pass)
    return;

  // End clear render pass. Bind() will call BeginRenderPass() which
  // will switch to the load/store render pass.
  EndRenderPass();
}

void StateTracker::UpdateDescriptorSet()
{
  if (m_pipeline->GetUsage() != AbstractPipelineUsage::Utility)
    UpdateGXDescriptorSet();
  else
    UpdateUtilityDescriptorSet();
}

void StateTracker::UpdateGXDescriptorSet()
{
  const size_t MAX_DESCRIPTOR_WRITES = NUM_UBO_DESCRIPTOR_SET_BINDINGS +  // UBO
                                       1 +                                // Samplers
                                       2;                                 // SSBO
  std::array<VkWriteDescriptorSet, MAX_DESCRIPTOR_WRITES> writes;
  u32 num_writes = 0;

  const bool needs_gs_ubo = g_ActiveConfig.backend_info.bSupportsGeometryShaders ||
                            g_ActiveConfig.UseVSForLinePointExpand();

  if (m_dirty_flags & DIRTY_FLAG_GX_UBOS || m_gx_descriptor_sets[0] == VK_NULL_HANDLE)
  {
    m_gx_descriptor_sets[0] = m_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_STANDARD_UNIFORM_BUFFERS));

    for (size_t i = 0; i < NUM_UBO_DESCRIPTOR_SET_BINDINGS; i++)
    {
      if (i == UBO_DESCRIPTOR_SET_BINDING_GS && !needs_gs_ubo)
      {
        continue;
      }

      // If custom pixel shaders haven't been used, their buffer range is 0
      if (i == UBO_DESCRIPTOR_SET_BINDING_PS_CUST && m_bindings.gx_ubo_bindings[i].range == 0)
      {
        continue;
      }

      writes[num_writes++] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              nullptr,
                              m_gx_descriptor_sets[0],
                              static_cast<uint32_t>(i),
                              0,
                              1,
                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                              nullptr,
                              &m_bindings.gx_ubo_bindings[i],
                              nullptr};
    }

    m_dirty_flags = (m_dirty_flags & ~DIRTY_FLAG_GX_UBOS) | DIRTY_FLAG_DESCRIPTOR_SETS;
  }

  if (m_dirty_flags & DIRTY_FLAG_GX_SAMPLERS || m_gx_descriptor_sets[1] == VK_NULL_HANDLE)
  {
    m_gx_descriptor_sets[1] = m_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_STANDARD_SAMPLERS));

    writes[num_writes++] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            nullptr,
                            m_gx_descriptor_sets[1],
                            0,
                            0,
                            static_cast<u32>(VideoCommon::MAX_PIXEL_SHADER_SAMPLERS),
                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            m_bindings.samplers.data(),
                            nullptr,
                            nullptr};
    m_dirty_flags = (m_dirty_flags & ~DIRTY_FLAG_GX_SAMPLERS) | DIRTY_FLAG_DESCRIPTOR_SETS;
  }

  const bool needs_bbox_ssbo = g_ActiveConfig.backend_info.bSupportsBBox;
  const bool needs_vertex_ssbo = (g_ActiveConfig.backend_info.bSupportsDynamicVertexLoader &&
                                  m_pipeline->GetUsage() == AbstractPipelineUsage::GXUber) ||
                                 g_ActiveConfig.UseVSForLinePointExpand();
  const bool needs_ssbo = needs_bbox_ssbo || needs_vertex_ssbo;

  if (needs_ssbo &&
      (m_dirty_flags & DIRTY_FLAG_GX_SSBO || m_gx_descriptor_sets[2] == VK_NULL_HANDLE))
  {
    m_gx_descriptor_sets[2] =
        m_command_buffer_mgr->AllocateDescriptorSet(g_object_cache->GetDescriptorSetLayout(
            DESCRIPTOR_SET_LAYOUT_STANDARD_SHADER_STORAGE_BUFFERS));

    writes[num_writes++] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_gx_descriptor_sets[2], 0,      0, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      nullptr, &m_bindings.ssbo,        nullptr};

    if (g_ActiveConfig.backend_info.bSupportsDynamicVertexLoader ||
        g_ActiveConfig.UseVSForLinePointExpand())
    {
      writes[num_writes++] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              nullptr,
                              m_gx_descriptor_sets[2],
                              1,
                              0,
                              1,
                              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              nullptr,
                              &m_bindings.gx_uber_vertex_ssbo,
                              nullptr};
    }

    m_dirty_flags = (m_dirty_flags & ~DIRTY_FLAG_GX_SSBO) | DIRTY_FLAG_DESCRIPTOR_SETS;
  }

  if (num_writes > 0)
    vkUpdateDescriptorSets(g_vulkan_context->GetDevice(), num_writes, writes.data(), 0, nullptr);

  if (m_dirty_flags & DIRTY_FLAG_DESCRIPTOR_SETS)
  {
    vkCmdBindDescriptorSets(m_command_buffer_mgr->GetCurrentCommandBuffer(),
                            VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetVkPipelineLayout(), 0,
                            needs_ssbo ? NUM_GX_DESCRIPTOR_SETS : (NUM_GX_DESCRIPTOR_SETS - 1),
                            m_gx_descriptor_sets.data(),
                            needs_gs_ubo ? NUM_UBO_DESCRIPTOR_SET_BINDINGS :
                                           (NUM_UBO_DESCRIPTOR_SET_BINDINGS - 1),
                            m_bindings.gx_ubo_offsets.data());
    m_dirty_flags &= ~(DIRTY_FLAG_DESCRIPTOR_SETS | DIRTY_FLAG_GX_UBO_OFFSETS);
  }
  else if (m_dirty_flags & DIRTY_FLAG_GX_UBO_OFFSETS)
  {
    vkCmdBindDescriptorSets(
        m_command_buffer_mgr->GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipeline->GetVkPipelineLayout(), 0, 1, m_gx_descriptor_sets.data(),
        needs_gs_ubo ? NUM_UBO_DESCRIPTOR_SET_BINDINGS : (NUM_UBO_DESCRIPTOR_SET_BINDINGS - 1),
        m_bindings.gx_ubo_offsets.data());
    m_dirty_flags &= ~DIRTY_FLAG_GX_UBO_OFFSETS;
  }
}

void StateTracker::UpdateUtilityDescriptorSet()
{
  // Max number of updates - UBO, Samplers, TexelBuffer
  std::array<VkWriteDescriptorSet, 3> dswrites;
  u32 writes = 0;

  // Allocate descriptor sets.
  if (m_dirty_flags & DIRTY_FLAG_UTILITY_UBO || m_utility_descriptor_sets[0] == VK_NULL_HANDLE)
  {
    m_utility_descriptor_sets[0] = m_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_UTILITY_UNIFORM_BUFFER));

    dswrites[writes++] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                          nullptr,
                          m_utility_descriptor_sets[0],
                          0,
                          0,
                          1,
                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                          nullptr,
                          &m_bindings.utility_ubo_binding,
                          nullptr};

    m_dirty_flags = (m_dirty_flags & ~DIRTY_FLAG_UTILITY_UBO) | DIRTY_FLAG_DESCRIPTOR_SETS;
  }

  if (m_dirty_flags & DIRTY_FLAG_UTILITY_BINDINGS || m_utility_descriptor_sets[1] == VK_NULL_HANDLE)
  {
    m_utility_descriptor_sets[1] = m_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_UTILITY_SAMPLERS));

    dswrites[writes++] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                          nullptr,
                          m_utility_descriptor_sets[1],
                          0,
                          0,
                          NUM_UTILITY_PIXEL_SAMPLERS,
                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          m_bindings.samplers.data(),
                          nullptr,
                          nullptr};
    dswrites[writes++] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                          nullptr,
                          m_utility_descriptor_sets[1],
                          8,
                          0,
                          1,
                          VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                          nullptr,
                          nullptr,
                          m_bindings.texel_buffers.data()};

    m_dirty_flags = (m_dirty_flags & ~DIRTY_FLAG_UTILITY_BINDINGS) | DIRTY_FLAG_DESCRIPTOR_SETS;
  }

  if (writes > 0)
    vkUpdateDescriptorSets(g_vulkan_context->GetDevice(), writes, dswrites.data(), 0, nullptr);

  if (m_dirty_flags & DIRTY_FLAG_DESCRIPTOR_SETS)
  {
    vkCmdBindDescriptorSets(m_command_buffer_mgr->GetCurrentCommandBuffer(),
                            VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetVkPipelineLayout(), 0,
                            NUM_UTILITY_DESCRIPTOR_SETS, m_utility_descriptor_sets.data(), 1,
                            &m_bindings.utility_ubo_offset);
    m_dirty_flags &= ~(DIRTY_FLAG_DESCRIPTOR_SETS | DIRTY_FLAG_UTILITY_UBO_OFFSET);
  }
  else if (m_dirty_flags & DIRTY_FLAG_UTILITY_UBO_OFFSET)
  {
    vkCmdBindDescriptorSets(m_command_buffer_mgr->GetCurrentCommandBuffer(),
                            VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetVkPipelineLayout(), 0,
                            1, m_utility_descriptor_sets.data(), 1, &m_bindings.utility_ubo_offset);
    m_dirty_flags &= ~(DIRTY_FLAG_DESCRIPTOR_SETS | DIRTY_FLAG_UTILITY_UBO_OFFSET);
  }
}

void StateTracker::UpdateComputeDescriptorSet()
{
  // Max number of updates - UBO, Samplers, TexelBuffer, Image
  std::array<VkWriteDescriptorSet, 4> dswrites;

  // Allocate descriptor sets.
  if (m_dirty_flags & DIRTY_FLAG_COMPUTE_BINDINGS)
  {
    m_compute_descriptor_set = m_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_COMPUTE));
    dswrites[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                   nullptr,
                   m_compute_descriptor_set,
                   0,
                   0,
                   1,
                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                   nullptr,
                   &m_bindings.utility_ubo_binding,
                   nullptr};
    dswrites[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                   nullptr,
                   m_compute_descriptor_set,
                   1,
                   0,
                   VideoCommon::MAX_COMPUTE_SHADER_SAMPLERS,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   m_bindings.samplers.data(),
                   nullptr,
                   nullptr};
    dswrites[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                   nullptr,
                   m_compute_descriptor_set,
                   1 + VideoCommon::MAX_COMPUTE_SHADER_SAMPLERS,
                   0,
                   NUM_COMPUTE_TEXEL_BUFFERS,
                   VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                   nullptr,
                   nullptr,
                   m_bindings.texel_buffers.data()};
    dswrites[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                   nullptr,
                   m_compute_descriptor_set,
                   1 + VideoCommon::MAX_COMPUTE_SHADER_SAMPLERS + NUM_COMPUTE_TEXEL_BUFFERS,
                   0,
                   VideoCommon::MAX_COMPUTE_SHADER_SAMPLERS,
                   VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                   m_bindings.image_textures.data(),
                   nullptr,
                   nullptr};

    vkUpdateDescriptorSets(g_vulkan_context->GetDevice(), static_cast<uint32_t>(dswrites.size()),
                           dswrites.data(), 0, nullptr);
    m_dirty_flags =
        (m_dirty_flags & ~DIRTY_FLAG_COMPUTE_BINDINGS) | DIRTY_FLAG_COMPUTE_DESCRIPTOR_SET;
  }

  if (m_dirty_flags & DIRTY_FLAG_COMPUTE_DESCRIPTOR_SET)
  {
    vkCmdBindDescriptorSets(m_command_buffer_mgr->GetCurrentCommandBuffer(),
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_COMPUTE), 0, 1,
                            &m_compute_descriptor_set, 1, &m_bindings.utility_ubo_offset);
    m_dirty_flags &= ~DIRTY_FLAG_COMPUTE_DESCRIPTOR_SET;
  }
}

}  // namespace Vulkan
