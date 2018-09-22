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
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/StagingBuffer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/TextureConfig.h"

namespace Vulkan
{
VKTexture::VKTexture(const TextureConfig& tex_config, std::unique_ptr<Texture2D> texture,
                     VkFramebuffer framebuffer)
    : AbstractTexture(tex_config), m_texture(std::move(texture)), m_framebuffer(framebuffer)
{
}

std::unique_ptr<VKTexture> VKTexture::Create(const TextureConfig& tex_config)
{
  // Determine image usage, we need to flag as an attachment if it can be used as a rendertarget.
  VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT;
  if (tex_config.rendertarget)
    usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  // Allocate texture object
  VkFormat vk_format = Util::GetVkFormatForHostTextureFormat(tex_config.format);
  auto texture =
      Texture2D::Create(tex_config.width, tex_config.height, tex_config.levels, tex_config.layers,
                        vk_format, static_cast<VkSampleCountFlagBits>(tex_config.samples),
                        VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL, usage);

  if (!texture)
  {
    return nullptr;
  }

  // If this is a render target (for efb copies), allocate a framebuffer
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  if (tex_config.rendertarget)
  {
    VkImageView framebuffer_attachments[] = {texture->GetView()};
    VkRenderPass render_pass =
        g_object_cache->GetRenderPass(texture->GetFormat(), VK_FORMAT_UNDEFINED, tex_config.samples,
                                      VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    VkFramebufferCreateInfo framebuffer_info = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        nullptr,
        0,
        render_pass,
        static_cast<u32>(ArraySize(framebuffer_attachments)),
        framebuffer_attachments,
        texture->GetWidth(),
        texture->GetHeight(),
        texture->GetLayers()};

    VkResult res = vkCreateFramebuffer(g_vulkan_context->GetDevice(), &framebuffer_info, nullptr,
                                       &framebuffer);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
      return nullptr;
    }

    if (!IsDepthFormat(tex_config.format))
    {
      // Clear render targets before use to prevent reading uninitialized memory.
      VkClearColorValue clear_value = {{0.0f, 0.0f, 0.0f, 1.0f}};
      VkImageSubresourceRange clear_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, tex_config.levels, 0,
                                             tex_config.layers};
      texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      vkCmdClearColorImage(g_command_buffer_mgr->GetCurrentInitCommandBuffer(), texture->GetImage(),
                           texture->GetLayout(), &clear_value, 1, &clear_range);
    }
    else
    {
      // Clear render targets before use to prevent reading uninitialized memory.
      VkClearDepthStencilValue clear_value = {0.0f, 0};
      VkImageSubresourceRange clear_range = {Util::GetImageAspectForFormat(vk_format), 0,
                                             tex_config.levels, 0, tex_config.layers};
      texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      vkCmdClearDepthStencilImage(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                  texture->GetImage(), texture->GetLayout(), &clear_value, 1,
                                  &clear_range);
    }
  }

  return std::unique_ptr<VKTexture>(new VKTexture(tex_config, std::move(texture), framebuffer));
}

VKTexture::~VKTexture()
{
  // Texture is automatically cleaned up, however, we don't want to leave it bound.
  g_renderer->UnbindTexture(this);
  if (m_framebuffer != VK_NULL_HANDLE)
    g_command_buffer_mgr->DeferFramebufferDestruction(m_framebuffer);
}

Texture2D* VKTexture::GetRawTexIdentifier() const
{
  return m_texture.get();
}
VkFramebuffer VKTexture::GetFramebuffer() const
{
  return m_framebuffer;
}

void VKTexture::CopyRectangleFromTexture(const AbstractTexture* src,
                                         const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                         u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                         u32 dst_layer, u32 dst_level)
{
  Texture2D* src_texture = static_cast<const VKTexture*>(src)->GetRawTexIdentifier();

  ASSERT_MSG(VIDEO,
             static_cast<u32>(src_rect.GetWidth()) <= src_texture->GetWidth() &&
                 static_cast<u32>(src_rect.GetHeight()) <= src_texture->GetHeight(),
             "Source rect is too large for CopyRectangleFromTexture");

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

  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkCmdCopyImage(g_command_buffer_mgr->GetCurrentCommandBuffer(), src_texture->GetImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_texture->GetImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

  // Ensure both textures remain in the SHADER_READ_ONLY layout so they can be bound.
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VKTexture::ScaleRectangleFromTexture(const AbstractTexture* source,
                                          const MathUtil::Rectangle<int>& src_rect,
                                          const MathUtil::Rectangle<int>& dst_rect)
{
  Texture2D* src_texture = static_cast<const VKTexture*>(source)->GetRawTexIdentifier();

  // Can't do this within a game render pass.
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->SetPendingRebind();

  // Can't render to a non-rendertarget (no framebuffer).
  ASSERT_MSG(VIDEO, m_config.rendertarget,
             "Destination texture for partial copy is not a rendertarget");

  // Render pass expects dst_texture to be in COLOR_ATTACHMENT_OPTIMAL state.
  // src_texture should already be in SHADER_READ_ONLY state, but transition in case (XFB).
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkRenderPass render_pass = g_object_cache->GetRenderPass(
      m_texture->GetFormat(), VK_FORMAT_UNDEFINED, 1, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD), render_pass,
                         g_shader_cache->GetPassthroughVertexShader(),
                         g_shader_cache->GetPassthroughGeometryShader(),
                         TextureCache::GetInstance()->GetCopyShader());

  VkRect2D region = {
      {dst_rect.left, dst_rect.top},
      {static_cast<u32>(dst_rect.GetWidth()), static_cast<u32>(dst_rect.GetHeight())}};
  draw.BeginRenderPass(m_framebuffer, region);
  draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetLinearSampler());
  draw.DrawQuad(dst_rect.left, dst_rect.top, dst_rect.GetWidth(), dst_rect.GetHeight(),
                src_rect.left, src_rect.top, 0, src_rect.GetWidth(), src_rect.GetHeight(),
                static_cast<int>(src_texture->GetWidth()),
                static_cast<int>(src_texture->GetHeight()));
  draw.EndRenderPass();

  // Ensure both textures remain in the SHADER_READ_ONLY layout so they can be bound.
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
  VkImageLayout old_src_layout = srcentry->m_texture->GetLayout();
  srcentry->m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkImageResolve resolve = {
      {VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1},                               // srcSubresource
      {rect.left, rect.top, 0},                                                   // srcOffset
      {VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1},                               // dstSubresource
      {rect.left, rect.top, 0},                                                   // dstOffset
      {static_cast<u32>(rect.GetWidth()), static_cast<u32>(rect.GetHeight()), 1}  // extent
  };
  vkCmdResolveImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                    srcentry->m_texture->GetImage(), srcentry->m_texture->GetLayout(),
                    m_texture->GetImage(), m_texture->GetLayout(), 1, &resolve);

  // Restore old source texture layout. Destination is assumed to be bound as a shader resource.
  srcentry->m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          old_src_layout);
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VKTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                     size_t buffer_size)
{
  // Can't copy data larger than the texture extents.
  width = std::max(1u, std::min(width, m_texture->GetWidth() >> level));
  height = std::max(1u, std::min(height, m_texture->GetHeight() >> level));

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
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // For unaligned textures, we can save some memory in the transfer buffer by skipping the rows
  // that lie outside of the texture's dimensions.
  u32 upload_alignment = static_cast<u32>(g_vulkan_context->GetBufferImageGranularity());
  u32 block_size = Util::GetBlockSize(m_texture->GetFormat());
  u32 num_rows = Common::AlignUp(height, block_size) / block_size;
  size_t source_pitch = CalculateStrideForFormat(m_config.format, row_length);
  size_t upload_size = source_pitch * num_rows;
  std::unique_ptr<StagingBuffer> temp_buffer;
  VkBuffer upload_buffer;
  VkDeviceSize upload_buffer_offset;

  // Does this texture data fit within the streaming buffer?
  if (upload_size <= STAGING_TEXTURE_UPLOAD_THRESHOLD &&
      upload_size <= MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE)
  {
    StreamBuffer* stream_buffer = TextureCache::GetInstance()->GetTextureUploadBuffer();
    if (!stream_buffer->ReserveMemory(upload_size, upload_alignment))
    {
      // Execute the command buffer first.
      WARN_LOG(VIDEO, "Executing command list while waiting for space in texture upload buffer");
      Util::ExecuteCurrentCommandsAndRestoreState(false);

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
                         m_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                         &image_copy);

  // Last mip level? We shouldn't be doing any further uploads now, so transition for rendering.
  if (level == (m_config.levels - 1))
  {
    m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
}

VKStagingTexture::VKStagingTexture(StagingTextureType type, const TextureConfig& config,
                                   std::unique_ptr<StagingBuffer> buffer)
    : AbstractStagingTexture(type, config), m_staging_buffer(std::move(buffer))
{
}

VKStagingTexture::~VKStagingTexture()
{
  if (m_needs_flush)
    VKStagingTexture::Flush();
}

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
  ASSERT(m_type == StagingTextureType::Readback || m_type == StagingTextureType::Mutable);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= src->GetConfig().width &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= src->GetConfig().height);
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= m_config.width &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= m_config.height);

  Texture2D* src_tex = static_cast<const VKTexture*>(src)->GetRawTexIdentifier();
  CopyFromTexture(src_tex, src_rect, src_layer, src_level, dst_rect);
}

void VKStagingTexture::CopyFromTexture(Texture2D* src, const MathUtil::Rectangle<int>& src_rect,
                                       u32 src_layer, u32 src_level,
                                       const MathUtil::Rectangle<int>& dst_rect)
{
  if (m_needs_flush)
  {
    // Drop copy before reusing it.
    g_command_buffer_mgr->RemoveFencePointCallback(this);
    m_flush_fence = VK_NULL_HANDLE;
    m_needs_flush = false;
  }

  VkImageLayout old_layout = src->GetLayout();
  src->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  // Issue the image->buffer copy, but delay it for now.
  VkBufferImageCopy image_copy = {};
  VkImageAspectFlags aspect =
      Util::IsDepthFormat(src->GetFormat()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  image_copy.bufferOffset =
      static_cast<VkDeviceSize>(static_cast<size_t>(dst_rect.top) * m_config.GetStride() +
                                static_cast<size_t>(dst_rect.left) * m_texel_size);
  image_copy.bufferRowLength = static_cast<u32>(m_config.width);
  image_copy.bufferImageHeight = 0;
  image_copy.imageSubresource = {aspect, src_level, src_layer, 1};
  image_copy.imageOffset = {src_rect.left, src_rect.top, 0};
  image_copy.imageExtent = {static_cast<u32>(src_rect.GetWidth()),
                            static_cast<u32>(src_rect.GetHeight()), 1u};
  vkCmdCopyImageToBuffer(g_command_buffer_mgr->GetCurrentCommandBuffer(), src->GetImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_staging_buffer->GetBuffer(), 1,
                         &image_copy);

  // Restore old source texture layout.
  src->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(), old_layout);

  m_needs_flush = true;
  g_command_buffer_mgr->AddFencePointCallback(this,
                                              [this](VkCommandBuffer buf, VkFence fence) {
                                                ASSERT(m_needs_flush);
                                                if (m_flush_fence != VK_NULL_HANDLE)
                                                  return;

                                                m_flush_fence = fence;
                                              },
                                              [this](VkFence fence) {
                                                if (m_flush_fence != fence)
                                                  return;

                                                m_flush_fence = VK_NULL_HANDLE;
                                                m_needs_flush = false;
                                                g_command_buffer_mgr->RemoveFencePointCallback(
                                                    this);
                                                m_staging_buffer->InvalidateCPUCache();
                                              });
}

void VKStagingTexture::CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                                     const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                                     u32 dst_level)
{
  ASSERT(m_type == StagingTextureType::Upload || m_type == StagingTextureType::Mutable);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= m_config.width &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= m_config.height);
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= dst->GetConfig().width &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= dst->GetConfig().height);

  if (m_needs_flush)
  {
    // Drop copy before reusing it.
    g_command_buffer_mgr->RemoveFencePointCallback(this);
    m_flush_fence = VK_NULL_HANDLE;
    m_needs_flush = false;
  }

  // Flush caches before copying.
  m_staging_buffer->FlushCPUCache();

  Texture2D* dst_tex = static_cast<const VKTexture*>(dst)->GetRawTexIdentifier();
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
  g_command_buffer_mgr->AddFencePointCallback(this,
                                              [this](VkCommandBuffer buf, VkFence fence) {
                                                ASSERT(m_needs_flush);
                                                if (m_flush_fence != VK_NULL_HANDLE)
                                                  return;

                                                m_flush_fence = fence;
                                              },
                                              [this](VkFence fence) {
                                                if (m_flush_fence != fence)
                                                  return;

                                                m_flush_fence = VK_NULL_HANDLE;
                                                m_needs_flush = false;
                                                g_command_buffer_mgr->RemoveFencePointCallback(
                                                    this);
                                              });
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

  // Either of the below two calls will cause the callback to fire.
  g_command_buffer_mgr->RemoveFencePointCallback(this);
  if (m_flush_fence != VK_NULL_HANDLE)
  {
    // WaitForFence should fire the callback.
    g_command_buffer_mgr->WaitForFence(m_flush_fence);
    m_flush_fence = VK_NULL_HANDLE;
  }
  else
  {
    // We don't have a fence, and are pending. That means the readback is in the current
    // command buffer, and must execute it to populate the staging texture.
    Util::ExecuteCurrentCommandsAndRestoreState(false, true);
  }
  m_needs_flush = false;

  // For readback textures, invalidate the CPU cache as there is new data there.
  if (m_type == StagingTextureType::Readback || m_type == StagingTextureType::Mutable)
    m_staging_buffer->InvalidateCPUCache();
}

VKFramebuffer::VKFramebuffer(const VKTexture* color_attachment, const VKTexture* depth_attachment,
                             u32 width, u32 height, u32 layers, u32 samples, VkFramebuffer fb,
                             VkRenderPass load_render_pass, VkRenderPass discard_render_pass,
                             VkRenderPass clear_render_pass)
    : AbstractFramebuffer(
          color_attachment ? color_attachment->GetFormat() : AbstractTextureFormat::Undefined,
          depth_attachment ? depth_attachment->GetFormat() : AbstractTextureFormat::Undefined,
          width, height, layers, samples),
      m_color_attachment(color_attachment), m_depth_attachment(depth_attachment), m_fb(fb),
      m_load_render_pass(load_render_pass), m_discard_render_pass(discard_render_pass),
      m_clear_render_pass(clear_render_pass)
{
}

VKFramebuffer::~VKFramebuffer()
{
  g_command_buffer_mgr->DeferFramebufferDestruction(m_fb);
}

std::unique_ptr<VKFramebuffer> VKFramebuffer::Create(const VKTexture* color_attachment,
                                                     const VKTexture* depth_attachment)
{
  if (!ValidateConfig(color_attachment, depth_attachment))
    return nullptr;

  const VkFormat vk_color_format =
      color_attachment ? color_attachment->GetRawTexIdentifier()->GetFormat() : VK_FORMAT_UNDEFINED;
  const VkFormat vk_depth_format =
      depth_attachment ? depth_attachment->GetRawTexIdentifier()->GetFormat() : VK_FORMAT_UNDEFINED;
  const VKTexture* either_attachment = color_attachment ? color_attachment : depth_attachment;
  const u32 width = either_attachment->GetWidth();
  const u32 height = either_attachment->GetHeight();
  const u32 layers = either_attachment->GetLayers();
  const u32 samples = either_attachment->GetSamples();

  std::array<VkImageView, 2> attachment_views{};
  u32 num_attachments = 0;

  if (color_attachment)
    attachment_views[num_attachments++] = color_attachment->GetRawTexIdentifier()->GetView();

  if (depth_attachment)
    attachment_views[num_attachments++] = depth_attachment->GetRawTexIdentifier()->GetView();

  VkRenderPass load_render_pass = g_object_cache->GetRenderPass(
      vk_color_format, vk_depth_format, samples, VK_ATTACHMENT_LOAD_OP_LOAD);
  VkRenderPass discard_render_pass = g_object_cache->GetRenderPass(
      vk_color_format, vk_depth_format, samples, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
  VkRenderPass clear_render_pass = g_object_cache->GetRenderPass(
      vk_color_format, vk_depth_format, samples, VK_ATTACHMENT_LOAD_OP_CLEAR);
  if (load_render_pass == VK_NULL_HANDLE || discard_render_pass == VK_NULL_HANDLE ||
      clear_render_pass == VK_NULL_HANDLE)
  {
    return nullptr;
  }

  VkFramebufferCreateInfo framebuffer_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                              nullptr,
                                              0,
                                              load_render_pass,
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

void VKFramebuffer::TransitionForRender() const
{
  if (m_color_attachment)
  {
    m_color_attachment->GetRawTexIdentifier()->TransitionToLayout(
        g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }

  if (m_depth_attachment)
  {
    m_depth_attachment->GetRawTexIdentifier()->TransitionToLayout(
        g_command_buffer_mgr->GetCurrentCommandBuffer(),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  }
}

void VKFramebuffer::TransitionForSample() const
{
  if (StateTracker::GetInstance()->GetFramebuffer() == m_fb)
    StateTracker::GetInstance()->EndRenderPass();

  if (m_color_attachment)
  {
    m_color_attachment->GetRawTexIdentifier()->TransitionToLayout(
        g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  if (m_depth_attachment)
  {
    m_depth_attachment->GetRawTexIdentifier()->TransitionToLayout(
        g_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
}

}  // namespace Vulkan
