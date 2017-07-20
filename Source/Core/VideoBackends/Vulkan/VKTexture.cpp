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
#include "VideoBackends/Vulkan/StagingTexture2D.h"
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
  auto texture = Texture2D::Create(tex_config.width, tex_config.height, tex_config.levels,
                                   tex_config.layers, vk_format, VK_SAMPLE_COUNT_1_BIT,
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
    VkFramebufferCreateInfo framebuffer_info = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        nullptr,
        0,
        TextureCache::GetInstance()->GetTextureCopyRenderPass(),
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

    // Clear render targets before use to prevent reading uninitialized memory.
    VkClearColorValue clear_value = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange clear_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, tex_config.levels, 0,
                                           tex_config.layers};
    texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdClearColorImage(g_command_buffer_mgr->GetCurrentInitCommandBuffer(), texture->GetImage(),
                         texture->GetLayout(), &clear_value, 1, &clear_range);
  }

  return std::unique_ptr<VKTexture>(new VKTexture(tex_config, std::move(texture), framebuffer));
}

VKTexture::~VKTexture()
{
  // Texture is automatically cleaned up, however, we don't want to leave it bound.
  StateTracker::GetInstance()->UnbindTexture(m_texture->GetView());
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

void VKTexture::Bind(unsigned int stage)
{
  // Texture should always be in SHADER_READ_ONLY layout prior to use.
  // This is so we don't need to transition during render passes.
  _assert_(m_texture->GetLayout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  StateTracker::GetInstance()->SetTexture(stage, m_texture->GetView());
}

bool VKTexture::Save(const std::string& filename, unsigned int level)
{
  _assert_(level < m_config.levels);

  // We can't dump compressed textures currently (it would mean drawing them to a RGBA8
  // framebuffer, and saving that). TextureCache does not call Save for custom textures
  // anyway, so this is fine for now.
  _assert_(m_config.format == AbstractTextureFormat::RGBA8);

  // Determine dimensions of image we want to save.
  u32 level_width = std::max(1u, m_config.width >> level);
  u32 level_height = std::max(1u, m_config.height >> level);

  // Use a temporary staging texture for the download. Certainly not optimal,
  // but since we have to idle the GPU anyway it doesn't really matter.
  std::unique_ptr<StagingTexture2D> staging_texture = StagingTexture2D::Create(
      STAGING_BUFFER_TYPE_READBACK, level_width, level_height, TEXTURECACHE_TEXTURE_FORMAT);

  // Transition image to transfer source, and invalidate the current state,
  // since we'll be executing the command buffer.
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  StateTracker::GetInstance()->EndRenderPass();

  // Copy to download buffer.
  staging_texture->CopyFromImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                 m_texture->GetImage(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
                                 level_width, level_height, level, 0);

  // Restore original state of texture.
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Block until the GPU has finished copying to the staging texture.
  Util::ExecuteCurrentCommandsAndRestoreState(false, true);

  // Map the staging texture so we can copy the contents out.
  if (!staging_texture->Map())
  {
    PanicAlert("Failed to map staging texture");
    return false;
  }

  // Write texture out to file.
  // It's okay to throw this texture away immediately, since we're done with it, and
  // we blocked until the copy completed on the GPU anyway.
  bool result = TextureToPng(reinterpret_cast<u8*>(staging_texture->GetMapPointer()),
                             static_cast<u32>(staging_texture->GetRowStride()), filename,
                             level_width, level_height);

  staging_texture->Unmap();
  return result;
}

void VKTexture::CopyTextureRectangle(const MathUtil::Rectangle<int>& dst_rect,
                                     Texture2D* src_texture,
                                     const MathUtil::Rectangle<int>& src_rect)
{
  _assert_msg_(VIDEO, static_cast<u32>(src_rect.GetWidth()) <= src_texture->GetWidth() &&
                          static_cast<u32>(src_rect.GetHeight()) <= src_texture->GetHeight(),
               "Source rect is too large for CopyRectangleFromTexture");

  _assert_msg_(VIDEO, static_cast<u32>(dst_rect.GetWidth()) <= m_config.width &&
                          static_cast<u32>(dst_rect.GetHeight()) <= m_config.height,
               "Dest rect is too large for CopyRectangleFromTexture");

  VkImageCopy image_copy = {
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
       src_texture->GetLayers()},        // VkImageSubresourceLayers    srcSubresource
      {src_rect.left, src_rect.top, 0},  // VkOffset3D                  srcOffset
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,  // VkImageSubresourceLayers    dstSubresource
       m_config.layers},
      {dst_rect.left, dst_rect.top, 0},  // VkOffset3D                  dstOffset
      {static_cast<uint32_t>(src_rect.GetWidth()), static_cast<uint32_t>(src_rect.GetHeight()),
       1}  // VkExtent3D                  extent
  };

  // Must be called outside of a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkCmdCopyImage(g_command_buffer_mgr->GetCurrentCommandBuffer(), src_texture->GetImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_texture->GetImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);
}

void VKTexture::ScaleTextureRectangle(const MathUtil::Rectangle<int>& dst_rect,
                                      Texture2D* src_texture,
                                      const MathUtil::Rectangle<int>& src_rect)
{
  // Can't do this within a game render pass.
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->SetPendingRebind();

  // Can't render to a non-rendertarget (no framebuffer).
  _assert_msg_(VIDEO, m_config.rendertarget,
               "Destination texture for partial copy is not a rendertarget");

  // Render pass expects dst_texture to be in COLOR_ATTACHMENT_OPTIMAL state.
  // src_texture should already be in SHADER_READ_ONLY state, but transition in case (XFB).
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD),
                         TextureCache::GetInstance()->GetTextureCopyRenderPass(),
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
}

void VKTexture::CopyRectangleFromTexture(const AbstractTexture* source,
                                         const MathUtil::Rectangle<int>& srcrect,
                                         const MathUtil::Rectangle<int>& dstrect)
{
  auto* raw_source_texture = static_cast<const VKTexture*>(source)->GetRawTexIdentifier();
  CopyRectangleFromTexture(raw_source_texture, srcrect, dstrect);
}

void VKTexture::CopyRectangleFromTexture(Texture2D* source, const MathUtil::Rectangle<int>& srcrect,
                                         const MathUtil::Rectangle<int>& dstrect)
{
  if (srcrect.GetWidth() == dstrect.GetWidth() && srcrect.GetHeight() == dstrect.GetHeight())
    CopyTextureRectangle(dstrect, source, srcrect);
  else
    ScaleTextureRectangle(dstrect, source, srcrect);

  // Ensure both textures remain in the SHADER_READ_ONLY layout so they can be bound.
  source->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
  size_t source_pitch = CalculateHostTextureLevelPitch(m_config.format, row_length);
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

}  // namespace Vulkan
