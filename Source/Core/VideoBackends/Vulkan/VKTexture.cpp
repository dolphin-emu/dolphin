// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StagingBuffer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
VKTexture::VKTexture(const TextureConfig& tex_config, VkDeviceMemory device_memory, VkImage image,
                     VkImageLayout layout /* = VK_IMAGE_LAYOUT_UNDEFINED */,
                     ComputeImageLayout compute_layout /* = ComputeImageLayout::Undefined */)
    : AbstractTexture(tex_config), m_device_memory(device_memory), m_image(image), m_layout(layout),
      m_compute_layout(compute_layout)
{
}

VKTexture::~VKTexture()
{
  StateTracker::GetInstance()->UnbindTexture(m_view);
  g_command_buffer_mgr->DeferImageViewDestruction(m_view);

  // If we don't have device memory allocated, the image is not owned by us (e.g. swapchain)
  if (m_device_memory != VK_NULL_HANDLE)
  {
    g_command_buffer_mgr->DeferImageDestruction(m_image);
    g_command_buffer_mgr->DeferDeviceMemoryDestruction(m_device_memory);
  }
}

std::unique_ptr<VKTexture> VKTexture::Create(const TextureConfig& tex_config)
{
  // Determine image usage, we need to flag as an attachment if it can be used as a rendertarget.
  VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT;
  if (tex_config.IsRenderTarget())
  {
    usage |= IsDepthFormat(tex_config.format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT :
                                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  if (tex_config.IsComputeImage())
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;

  VkImageCreateInfo image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                  nullptr,
                                  0,
                                  VK_IMAGE_TYPE_2D,
                                  GetVkFormatForHostTextureFormat(tex_config.format),
                                  {tex_config.width, tex_config.height, 1},
                                  tex_config.levels,
                                  tex_config.layers,
                                  static_cast<VkSampleCountFlagBits>(tex_config.samples),
                                  VK_IMAGE_TILING_OPTIMAL,
                                  usage,
                                  VK_SHARING_MODE_EXCLUSIVE,
                                  0,
                                  nullptr,
                                  VK_IMAGE_LAYOUT_UNDEFINED};
  VkImage image;
  VkDeviceMemory device_memory;
  VkResult res = g_vulkan_context->Allocate(&image_info, &image, &device_memory);
  if (res != VK_SUCCESS)
  {
    return nullptr;
  }

  std::unique_ptr<VKTexture> texture = std::make_unique<VKTexture>(
      tex_config, device_memory, image, VK_IMAGE_LAYOUT_UNDEFINED, ComputeImageLayout::Undefined);
  if (!texture->CreateView(VK_IMAGE_VIEW_TYPE_2D_ARRAY))
    return nullptr;

  return texture;
}

std::unique_ptr<VKTexture> VKTexture::CreateAdopted(const TextureConfig& tex_config, VkImage image,
                                                    VkImageViewType view_type, VkImageLayout layout)
{
  std::unique_ptr<VKTexture> texture = std::make_unique<VKTexture>(
      tex_config, nullptr, image, layout, ComputeImageLayout::Undefined);
  if (!texture->CreateView(view_type))
    return nullptr;

  return texture;
}

bool VKTexture::CreateView(VkImageViewType type)
{
  VkImageViewCreateInfo view_info = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      nullptr,
      0,
      m_image,
      type,
      GetVkFormat(),
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {GetImageAspectForFormat(GetFormat()), 0, GetLevels(), 0, GetLayers()}};

  VkResult res = vkCreateImageView(g_vulkan_context->GetDevice(), &view_info, nullptr, &m_view);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
    return false;
  }

  return true;
}

VkFormat VKTexture::GetLinearFormat(VkFormat format)
{
  switch (format)
  {
  case VK_FORMAT_R8_SRGB:
    return VK_FORMAT_R8_UNORM;
  case VK_FORMAT_R8G8_SRGB:
    return VK_FORMAT_R8G8_UNORM;
  case VK_FORMAT_R8G8B8_SRGB:
    return VK_FORMAT_R8G8B8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case VK_FORMAT_B8G8R8_SRGB:
    return VK_FORMAT_B8G8R8_UNORM;
  case VK_FORMAT_B8G8R8A8_SRGB:
    return VK_FORMAT_B8G8R8A8_UNORM;
  default:
    return format;
  }
}

VkFormat VKTexture::GetVkFormatForHostTextureFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
    return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;

  case AbstractTextureFormat::DXT3:
    return VK_FORMAT_BC2_UNORM_BLOCK;

  case AbstractTextureFormat::DXT5:
    return VK_FORMAT_BC3_UNORM_BLOCK;

  case AbstractTextureFormat::BPTC:
    return VK_FORMAT_BC7_UNORM_BLOCK;

  case AbstractTextureFormat::RGBA8:
    return VK_FORMAT_R8G8B8A8_UNORM;

  case AbstractTextureFormat::BGRA8:
    return VK_FORMAT_B8G8R8A8_UNORM;

  case AbstractTextureFormat::R16:
    return VK_FORMAT_R16_UNORM;

  case AbstractTextureFormat::D16:
    return VK_FORMAT_D16_UNORM;

  case AbstractTextureFormat::D24_S8:
    return VK_FORMAT_D24_UNORM_S8_UINT;

  case AbstractTextureFormat::R32F:
    return VK_FORMAT_R32_SFLOAT;

  case AbstractTextureFormat::D32F:
    return VK_FORMAT_D32_SFLOAT;

  case AbstractTextureFormat::D32F_S8:
    return VK_FORMAT_D32_SFLOAT_S8_UINT;

  case AbstractTextureFormat::Undefined:
    return VK_FORMAT_UNDEFINED;

  default:
    PanicAlert("Unhandled texture format.");
    return VK_FORMAT_R8G8B8A8_UNORM;
  }
}

VkImageAspectFlags VKTexture::GetImageAspectForFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::D24_S8:
  case AbstractTextureFormat::D32F_S8:
    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

  case AbstractTextureFormat::D16:
  case AbstractTextureFormat::D32F:
    return VK_IMAGE_ASPECT_DEPTH_BIT;

  default:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

void VKTexture::CopyRectangleFromTexture(const AbstractTexture* src,
                                         const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                         u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                         u32 dst_layer, u32 dst_level)
{
  const VKTexture* src_texture = static_cast<const VKTexture*>(src);

  ASSERT_MSG(VIDEO,
             static_cast<u32>(src_rect.GetWidth()) <= src_texture->GetWidth() &&
                 static_cast<u32>(src_rect.GetHeight()) <= src_texture->GetHeight(),
             "Source rect(%d, %d) is too large for CopyRectangleFromTexture(%d, %d)",
             src_rect.GetWidth(), src_rect.GetHeight(),
             src_texture->GetWidth(), src_texture->GetHeight());

  ASSERT_MSG(VIDEO,
             static_cast<u32>(dst_rect.GetWidth()) <= m_config.width &&
                 static_cast<u32>(dst_rect.GetHeight()) <= m_config.height,
             "Dest rect is too large for CopyRectangleFromTexture");

  VkImageCopy image_copy = {
      {VK_IMAGE_ASPECT_COLOR_BIT, src_level, src_layer, src_texture->GetLayers()},
      {src_rect.left, src_rect.top, 0},
      {VK_IMAGE_ASPECT_COLOR_BIT, dst_level, dst_layer, m_config.layers},
      {dst_rect.left, dst_rect.top, 0},
      {static_cast<uint32_t>(src_rect.GetWidth()), static_cast<uint32_t>(src_rect.GetHeight()), 1}};

  // Must be called outside of a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  const VkImageLayout old_src_layout = src_texture->GetLayout();
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkCmdCopyImage(g_command_buffer_mgr->GetCurrentCommandBuffer(), src_texture->m_image,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_image,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

  // Only restore the source layout. Destination is restored by FinishedRendering().
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(), old_src_layout);
}

void VKTexture::ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                                   u32 layer, u32 level)
{
  const VKTexture* srcentry = static_cast<const VKTexture*>(src);
  DEBUG_ASSERT(m_config.samples == 1 && m_config.width == srcentry->m_config.width &&
               m_config.height == srcentry->m_config.height && srcentry->m_config.samples > 1);
  DEBUG_ASSERT(rect.left + rect.GetWidth() <= static_cast<int>(srcentry->m_config.width) &&
               rect.top + rect.GetHeight() <= static_cast<int>(srcentry->m_config.height));

  // Resolving is considered to be a transfer operation.
  StateTracker::GetInstance()->EndRenderPass();
  VkImageLayout old_src_layout = srcentry->m_layout;
  srcentry->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkImageResolve resolve = {
      {VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1},                               // srcSubresource
      {rect.left, rect.top, 0},                                                   // srcOffset
      {VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1},                               // dstSubresource
      {rect.left, rect.top, 0},                                                   // dstOffset
      {static_cast<u32>(rect.GetWidth()), static_cast<u32>(rect.GetHeight()), 1}  // extent
  };
  vkCmdResolveImage(g_command_buffer_mgr->GetCurrentCommandBuffer(), srcentry->m_image,
                    srcentry->m_layout, m_image, m_layout, 1, &resolve);

  srcentry->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(), old_src_layout);
}

void VKTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                     size_t buffer_size)
{
  // Can't copy data larger than the texture extents.
  width = std::max(1u, std::min(width, GetWidth() >> level));
  height = std::max(1u, std::min(height, GetHeight() >> level));

  // We don't care about the existing contents of the texture, so we could the image layout to
  // VK_IMAGE_LAYOUT_UNDEFINED here. However, under section 2.2.1, Queue Operation of the Vulkan
  // specification, it states:
  //
  //   Command buffer submissions to a single queue must always adhere to command order and
  //   API order, but otherwise may overlap or execute out of order.
  //
  // Therefore, if a previous frame's command buffer is still sampling from this texture, and we
  // overwrite it without a pipeline barrier, a texture sample could occur in parallel with the
  // texture upload/copy. I'm not sure if any drivers currently take advantage of this, but we
  // should insert an explicit pipeline barrier just in case (done by TransitionToLayout).
  //
  // We transition to TRANSFER_DST, ready for the image copy, and leave the texture in this state.
  // When the last mip level is uploaded, we transition to SHADER_READ_ONLY, ready for use. This is
  // because we can't transition in a render pass, and we don't necessarily know when this texture
  // is going to be used.
  TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // For unaligned textures, we can save some memory in the transfer buffer by skipping the rows
  // that lie outside of the texture's dimensions.
  const u32 upload_alignment = static_cast<u32>(g_vulkan_context->GetBufferImageGranularity());
  const u32 block_size = GetBlockSizeForFormat(GetFormat());
  const u32 num_rows = Common::AlignUp(height, block_size) / block_size;
  const u32 source_pitch = CalculateStrideForFormat(m_config.format, row_length);
  const u32 upload_size = source_pitch * num_rows;
  std::unique_ptr<StagingBuffer> temp_buffer;
  VkBuffer upload_buffer;
  VkDeviceSize upload_buffer_offset;

  // Does this texture data fit within the streaming buffer?
  if (upload_size <= STAGING_TEXTURE_UPLOAD_THRESHOLD)
  {
    StreamBuffer* stream_buffer = g_object_cache->GetTextureUploadBuffer();
    if (!stream_buffer->ReserveMemory(upload_size, upload_alignment))
    {
      // Execute the command buffer first.
      WARN_LOG(VIDEO, "Executing command list while waiting for space in texture upload buffer");
      Renderer::GetInstance()->ExecuteCommandBuffer(false);

      // Try allocating again. This may cause a fence wait.
      if (!stream_buffer->ReserveMemory(upload_size, upload_alignment))
        PanicAlert("Failed to allocate space in texture upload buffer");
    }
    // Copy to the streaming buffer.
    upload_buffer = stream_buffer->GetBuffer();
    upload_buffer_offset = stream_buffer->GetCurrentOffset();
    std::memcpy(stream_buffer->GetCurrentHostPointer(), buffer, upload_size);
    stream_buffer->CommitMemory(upload_size);
  }
  else
  {
    // Create a temporary staging buffer that is destroyed after the image is copied.
    temp_buffer = StagingBuffer::Create(STAGING_BUFFER_TYPE_UPLOAD, upload_size,
                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    if (!temp_buffer || !temp_buffer->Map())
    {
      PanicAlert("Failed to allocate staging texture for large texture upload.");
      return;
    }

    upload_buffer = temp_buffer->GetBuffer();
    upload_buffer_offset = 0;
    temp_buffer->Write(0, buffer, upload_size, true);
    temp_buffer->Unmap();
  }

  // Copy from the streaming buffer to the actual image.
  VkBufferImageCopy image_copy = {
      upload_buffer_offset,                      // VkDeviceSize                bufferOffset
      row_length,                                // uint32_t                    bufferRowLength
      0,                                         // uint32_t                    bufferImageHeight
      {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1},  // VkImageSubresourceLayers    imageSubresource
      {0, 0, 0},                                 // VkOffset3D                  imageOffset
      {width, height, 1}                         // VkExtent3D                  imageExtent
  };
  vkCmdCopyBufferToImage(g_command_buffer_mgr->GetCurrentInitCommandBuffer(), upload_buffer,
                         m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

  // Preemptively transition to shader read only after uploading the last mip level, as we're
  // likely finished with writes to this texture for now. We can't do this in common with a
  // FinishedRendering() call because the upload happens in the init command buffer, and we
  // don't want to interrupt the render pass with calls which were executed ages before.
  if (level == (m_config.levels - 1))
  {
    TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
}

void VKTexture::FinishedRendering()
{
  if (m_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    return;

  StateTracker::GetInstance()->EndRenderPass();
  TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VKTexture::OverrideImageLayout(VkImageLayout new_layout)
{
  m_layout = new_layout;
}

void VKTexture::TransitionToLayout(VkCommandBuffer command_buffer, VkImageLayout new_layout) const
{
  if (m_layout == new_layout)
    return;

  VkImageMemoryBarrier barrier = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType            sType
      nullptr,                                 // const void*                pNext
      0,                                       // VkAccessFlags              srcAccessMask
      0,                                       // VkAccessFlags              dstAccessMask
      m_layout,                                // VkImageLayout              oldLayout
      new_layout,                              // VkImageLayout              newLayout
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   srcQueueFamilyIndex
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   dstQueueFamilyIndex
      m_image,                                 // VkImage                    image
      {GetImageAspectForFormat(GetFormat()), 0, GetLevels(), 0,
       GetLayers()}  // VkImageSubresourceRange    subresourceRange
  };

  // srcStageMask -> Stages that must complete before the barrier
  // dstStageMask -> Stages that must wait for after the barrier before beginning
  VkPipelineStageFlags srcStageMask, dstStageMask;
  switch (m_layout)
  {
  case VK_IMAGE_LAYOUT_UNDEFINED:
    // Layout undefined therefore contents undefined, and we don't care what happens to it.
    barrier.srcAccessMask = 0;
    srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    break;

  case VK_IMAGE_LAYOUT_PREINITIALIZED:
    // Image has been pre-initialized by the host, so ensure all writes have completed.
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    srcStageMask = VK_PIPELINE_STAGE_HOST_BIT;
    break;

  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    // Image was being used as a color attachment, so ensure all writes have completed.
    barrier.srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    break;

  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    // Image was being used as a depthstencil attachment, so ensure all writes have completed.
    barrier.srcAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    srcStageMask =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    break;

  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    // Image was being used as a shader resource, make sure all reads have finished.
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    // Image was being used as a copy source, ensure all reads have finished.
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    // Image was being used as a copy destination, ensure all writes have finished.
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    break;

  default:
    srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    break;
  }

  switch (new_layout)
  {
  case VK_IMAGE_LAYOUT_UNDEFINED:
    barrier.dstAccessMask = 0;
    dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    break;

  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    barrier.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    break;

  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    barrier.dstAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dstStageMask =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    break;

  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    break;

  case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
    srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    break;

  default:
    dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    break;
  }

  // If we were using a compute layout, the stages need to reflect that
  switch (m_compute_layout)
  {
  case ComputeImageLayout::Undefined:
    break;
  case ComputeImageLayout::ReadOnly:
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    break;
  case ComputeImageLayout::WriteOnly:
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    break;
  case ComputeImageLayout::ReadWrite:
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    break;
  }
  m_compute_layout = ComputeImageLayout::Undefined;

  vkCmdPipelineBarrier(command_buffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);

  m_layout = new_layout;
}

void VKTexture::TransitionToLayout(VkCommandBuffer command_buffer,
                                   ComputeImageLayout new_layout) const
{
  ASSERT(new_layout != ComputeImageLayout::Undefined);
  if (m_compute_layout == new_layout)
    return;

  VkImageMemoryBarrier barrier = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType            sType
      nullptr,                                 // const void*                pNext
      0,                                       // VkAccessFlags              srcAccessMask
      0,                                       // VkAccessFlags              dstAccessMask
      m_layout,                                // VkImageLayout              oldLayout
      VK_IMAGE_LAYOUT_GENERAL,                 // VkImageLayout              newLayout
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   srcQueueFamilyIndex
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   dstQueueFamilyIndex
      m_image,                                 // VkImage                    image
      {GetImageAspectForFormat(GetFormat()), 0, GetLevels(), 0,
       GetLayers()}  // VkImageSubresourceRange    subresourceRange
  };

  VkPipelineStageFlags srcStageMask, dstStageMask;
  switch (m_layout)
  {
  case VK_IMAGE_LAYOUT_UNDEFINED:
    // Layout undefined therefore contents undefined, and we don't care what happens to it.
    barrier.srcAccessMask = 0;
    srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    break;

  case VK_IMAGE_LAYOUT_PREINITIALIZED:
    // Image has been pre-initialized by the host, so ensure all writes have completed.
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    srcStageMask = VK_PIPELINE_STAGE_HOST_BIT;
    break;

  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    // Image was being used as a color attachment, so ensure all writes have completed.
    barrier.srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    break;

  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    // Image was being used as a depthstencil attachment, so ensure all writes have completed.
    barrier.srcAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    srcStageMask =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    break;

  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    // Image was being used as a shader resource, make sure all reads have finished.
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    // Image was being used as a copy source, ensure all reads have finished.
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    // Image was being used as a copy destination, ensure all writes have finished.
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    break;

  default:
    srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    break;
  }

  switch (new_layout)
  {
  case ComputeImageLayout::ReadOnly:
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    break;
  case ComputeImageLayout::WriteOnly:
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    break;
  case ComputeImageLayout::ReadWrite:
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    break;
  default:
    dstStageMask = 0;
    break;
  }

  m_layout = barrier.newLayout;
  m_compute_layout = new_layout;

  vkCmdPipelineBarrier(command_buffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
}

VKStagingTexture::VKStagingTexture(StagingTextureType type, const TextureConfig& config,
                                   std::unique_ptr<StagingBuffer> buffer)
    : AbstractStagingTexture(type, config), m_staging_buffer(std::move(buffer))
{
}

VKStagingTexture::~VKStagingTexture() = default;

std::unique_ptr<VKStagingTexture> VKStagingTexture::Create(StagingTextureType type,
                                                           const TextureConfig& config)
{
  size_t stride = config.GetStride();
  size_t buffer_size = stride * static_cast<size_t>(config.height);

  STAGING_BUFFER_TYPE buffer_type;
  VkImageUsageFlags buffer_usage;
  if (type == StagingTextureType::Readback)
  {
    buffer_type = STAGING_BUFFER_TYPE_READBACK;
    buffer_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  else if (type == StagingTextureType::Upload)
  {
    buffer_type = STAGING_BUFFER_TYPE_UPLOAD;
    buffer_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  else
  {
    buffer_type = STAGING_BUFFER_TYPE_READBACK;
    buffer_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  VkBuffer buffer;
  VkDeviceMemory memory;
  bool coherent;
  if (!StagingBuffer::AllocateBuffer(buffer_type, buffer_size, buffer_usage, &buffer, &memory,
                                     &coherent))
  {
    return nullptr;
  }

  std::unique_ptr<StagingBuffer> staging_buffer =
      std::make_unique<StagingBuffer>(buffer_type, buffer, memory, buffer_size, coherent);
  std::unique_ptr<VKStagingTexture> staging_tex = std::unique_ptr<VKStagingTexture>(
      new VKStagingTexture(type, config, std::move(staging_buffer)));

  // Use persistent mapping.
  if (!staging_tex->m_staging_buffer->Map())
    return nullptr;
  staging_tex->m_map_pointer = staging_tex->m_staging_buffer->GetMapPointer();
  staging_tex->m_map_stride = stride;
  return staging_tex;
}

void VKStagingTexture::CopyFromTexture(const AbstractTexture* src,
                                       const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                       u32 src_level, const MathUtil::Rectangle<int>& dst_rect)
{
  const VKTexture* src_tex = static_cast<const VKTexture*>(src);
  ASSERT(m_type == StagingTextureType::Readback || m_type == StagingTextureType::Mutable);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= src_tex->GetWidth() &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= src_tex->GetHeight());
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= m_config.width &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= m_config.height);

  StateTracker::GetInstance()->EndRenderPass();

  VkImageLayout old_layout = src_tex->GetLayout();
  src_tex->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  // Issue the image->buffer copy, but delay it for now.
  VkBufferImageCopy image_copy = {};
  const VkImageAspectFlags aspect = VKTexture::GetImageAspectForFormat(src_tex->GetFormat());
  image_copy.bufferOffset =
      static_cast<VkDeviceSize>(static_cast<size_t>(dst_rect.top) * m_config.GetStride() +
                                static_cast<size_t>(dst_rect.left) * m_texel_size);
  image_copy.bufferRowLength = static_cast<u32>(m_config.width);
  image_copy.bufferImageHeight = 0;
  image_copy.imageSubresource = {aspect, src_level, src_layer, 1};
  image_copy.imageOffset = {src_rect.left, src_rect.top, 0};
  image_copy.imageExtent = {static_cast<u32>(src_rect.GetWidth()),
                            static_cast<u32>(src_rect.GetHeight()), 1u};
  vkCmdCopyImageToBuffer(g_command_buffer_mgr->GetCurrentCommandBuffer(), src_tex->GetImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_staging_buffer->GetBuffer(), 1,
                         &image_copy);

  // Restore old source texture layout.
  src_tex->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(), old_layout);

  m_needs_flush = true;
  m_flush_fence_counter = g_command_buffer_mgr->GetCurrentFenceCounter();
}

void VKStagingTexture::CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                                     const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                                     u32 dst_level)
{
  const VKTexture* dst_tex = static_cast<const VKTexture*>(dst);
  ASSERT(m_type == StagingTextureType::Upload || m_type == StagingTextureType::Mutable);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= m_config.width &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= m_config.height);
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= dst_tex->GetWidth() &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= dst_tex->GetHeight());

  // Flush caches before copying.
  m_staging_buffer->FlushCPUCache();
  StateTracker::GetInstance()->EndRenderPass();

  VkImageLayout old_layout = dst_tex->GetLayout();
  dst_tex->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // Issue the image->buffer copy, but delay it for now.
  VkBufferImageCopy image_copy = {};
  image_copy.bufferOffset =
      static_cast<VkDeviceSize>(static_cast<size_t>(src_rect.top) * m_config.GetStride() +
                                static_cast<size_t>(src_rect.left) * m_texel_size);
  image_copy.bufferRowLength = static_cast<u32>(m_config.width);
  image_copy.bufferImageHeight = 0;
  image_copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, dst_level, dst_layer, 1};
  image_copy.imageOffset = {dst_rect.left, dst_rect.top, 0};
  image_copy.imageExtent = {static_cast<u32>(dst_rect.GetWidth()),
                            static_cast<u32>(dst_rect.GetHeight()), 1u};
  vkCmdCopyBufferToImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         m_staging_buffer->GetBuffer(), dst_tex->GetImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

  // Restore old source texture layout.
  dst_tex->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(), old_layout);

  m_needs_flush = true;
  m_flush_fence_counter = g_command_buffer_mgr->GetCurrentFenceCounter();
}

bool VKStagingTexture::Map()
{
  // Always mapped.
  return true;
}

void VKStagingTexture::Unmap()
{
  // Always mapped.
}

void VKStagingTexture::Flush()
{
  if (!m_needs_flush)
    return;

  // Is this copy in the current command buffer?
  if (g_command_buffer_mgr->GetCurrentFenceCounter() == m_flush_fence_counter)
  {
    // Execute the command buffer and wait for it to finish.
    Renderer::GetInstance()->ExecuteCommandBuffer(false, true);
  }
  else
  {
    // Wait for the GPU to finish with it.
    g_command_buffer_mgr->WaitForFenceCounter(m_flush_fence_counter);
  }

  // For readback textures, invalidate the CPU cache as there is new data there.
  if (m_type == StagingTextureType::Readback || m_type == StagingTextureType::Mutable)
    m_staging_buffer->InvalidateCPUCache();

  m_needs_flush = false;
}

VKFramebuffer::VKFramebuffer(VKTexture* color_attachment, VKTexture* depth_attachment, u32 width,
                             u32 height, u32 layers, u32 samples, VkFramebuffer fb,
                             VkRenderPass load_render_pass, VkRenderPass discard_render_pass,
                             VkRenderPass clear_render_pass)
    : AbstractFramebuffer(
          color_attachment, depth_attachment,
          color_attachment ? color_attachment->GetFormat() : AbstractTextureFormat::Undefined,
          depth_attachment ? depth_attachment->GetFormat() : AbstractTextureFormat::Undefined,
          width, height, layers, samples),
      m_fb(fb), m_load_render_pass(load_render_pass), m_discard_render_pass(discard_render_pass),
      m_clear_render_pass(clear_render_pass)
{
}

VKFramebuffer::~VKFramebuffer()
{
  g_command_buffer_mgr->DeferFramebufferDestruction(m_fb);
}

std::unique_ptr<VKFramebuffer> VKFramebuffer::Create(VKTexture* color_attachment,
                                                     VKTexture* depth_attachment)
{
  if (!ValidateConfig(color_attachment, depth_attachment))
    return nullptr;

  const VkFormat vk_color_format =
      color_attachment ? color_attachment->GetVkFormat() : VK_FORMAT_UNDEFINED;
  const VkFormat vk_depth_format =
      depth_attachment ? depth_attachment->GetVkFormat() : VK_FORMAT_UNDEFINED;
  const VKTexture* either_attachment = color_attachment ? color_attachment : depth_attachment;
  const u32 width = either_attachment->GetWidth();
  const u32 height = either_attachment->GetHeight();
  const u32 layers = either_attachment->GetLayers();
  const u32 samples = either_attachment->GetSamples();

  std::array<VkImageView, 2> attachment_views{};
  u32 num_attachments = 0;
  VkRenderPass render_pass;
  VkRenderPass load_render_pass = VK_NULL_HANDLE;
  VkRenderPass discard_render_pass = VK_NULL_HANDLE;
  VkRenderPass clear_render_pass = VK_NULL_HANDLE;

  if (color_attachment)
    attachment_views[num_attachments++] = color_attachment->GetView();

  if (depth_attachment)
  {
    attachment_views[num_attachments++] = depth_attachment->GetView();
    load_render_pass = g_object_cache->GetRenderPass(
      vk_color_format, vk_depth_format, samples, VK_ATTACHMENT_LOAD_OP_LOAD);
    render_pass = load_render_pass;
  }
  else
  {
    discard_render_pass = g_object_cache->GetRenderPass(
      vk_color_format, vk_depth_format, samples, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    render_pass = discard_render_pass;
  }

  if (render_pass == VK_NULL_HANDLE)
  {
    return nullptr;
  }

  VkFramebufferCreateInfo framebuffer_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                              nullptr,
                                              0,
                                              render_pass,
                                              num_attachments,
                                              attachment_views.data(),
                                              width,
                                              height,
                                              layers};

  VkFramebuffer fb;
  VkResult res =
      vkCreateFramebuffer(g_vulkan_context->GetDevice(), &framebuffer_info, nullptr, &fb);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
    return nullptr;
  }

  return std::make_unique<VKFramebuffer>(color_attachment, depth_attachment, width, height, layers,
                                         samples, fb, load_render_pass, discard_render_pass,
                                         clear_render_pass);
}

VkRenderPass VKFramebuffer::GetLoadRenderPass()
{
  if(m_load_render_pass == VK_NULL_HANDLE)
  {
    const VkFormat vk_color_format = VKTexture::GetVkFormatForHostTextureFormat(m_color_format);
    const VkFormat vk_depth_format = VKTexture::GetVkFormatForHostTextureFormat(m_depth_format);
    m_load_render_pass = g_object_cache->GetRenderPass(
      vk_color_format, vk_depth_format, m_samples, VK_ATTACHMENT_LOAD_OP_LOAD);
  }
  return m_load_render_pass;
}

VkRenderPass VKFramebuffer::GetDiscardRenderPass()
{
  if(m_discard_render_pass == VK_NULL_HANDLE)
  {
    const VkFormat vk_color_format = VKTexture::GetVkFormatForHostTextureFormat(m_color_format);
    const VkFormat vk_depth_format = VKTexture::GetVkFormatForHostTextureFormat(m_depth_format);
    m_discard_render_pass = g_object_cache->GetRenderPass(
      vk_color_format, vk_depth_format, m_samples, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
  }
  return m_discard_render_pass;
}

VkRenderPass VKFramebuffer::GetClearRenderPass()
{
  if(m_clear_render_pass == VK_NULL_HANDLE)
  {
    const VkFormat vk_color_format = VKTexture::GetVkFormatForHostTextureFormat(m_color_format);
    const VkFormat vk_depth_format = VKTexture::GetVkFormatForHostTextureFormat(m_depth_format);
    m_clear_render_pass = g_object_cache->GetRenderPass(
      vk_color_format, vk_depth_format, m_samples, VK_ATTACHMENT_LOAD_OP_CLEAR);
  }
  return m_clear_render_pass;
}

void VKFramebuffer::TransitionForRender()
{
  if (m_color_attachment)
  {
    static_cast<VKTexture*>(m_color_attachment)
        ->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }

  if (m_depth_attachment)
  {
    static_cast<VKTexture*>(m_depth_attachment)
        ->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  }
}
}  // namespace Vulkan
