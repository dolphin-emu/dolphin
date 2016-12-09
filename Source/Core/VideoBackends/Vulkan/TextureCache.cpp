// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/TextureCache.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/TextureConverter.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/ImageWrite.h"

namespace Vulkan
{
TextureCache::TextureCache()
{
}

TextureCache::~TextureCache()
{
  if (m_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_render_pass, nullptr);
  TextureCache::DeleteShaders();
}

TextureCache* TextureCache::GetInstance()
{
  return static_cast<TextureCache*>(g_texture_cache.get());
}

bool TextureCache::Initialize()
{
  m_texture_upload_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, INITIAL_TEXTURE_UPLOAD_BUFFER_SIZE,
                           MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE);
  if (!m_texture_upload_buffer)
  {
    PanicAlert("Failed to create texture upload buffer");
    return false;
  }

  if (!CreateRenderPasses())
  {
    PanicAlert("Failed to create copy render pass");
    return false;
  }

  m_texture_converter = std::make_unique<TextureConverter>();
  if (!m_texture_converter->Initialize())
  {
    PanicAlert("Failed to initialize texture converter");
    return false;
  }

  if (!CompileShaders())
  {
    PanicAlert("Failed to compile one or more shaders");
    return false;
  }

  return true;
}

void TextureCache::ConvertTexture(TCacheEntryBase* base_entry, TCacheEntryBase* base_unconverted,
                                  void* palette, TlutFormat format)
{
  TCacheEntry* entry = static_cast<TCacheEntry*>(base_entry);
  TCacheEntry* unconverted = static_cast<TCacheEntry*>(base_unconverted);

  m_texture_converter->ConvertTexture(entry, unconverted, m_render_pass, palette, format);
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row,
                           u32 num_blocks_y, u32 memory_stride, bool is_depth_copy,
                           const EFBRectangle& src_rect, bool is_intensity, bool scale_by_half)
{
  // Flush EFB pokes first, as they're expected to be included.
  FramebufferManager::GetInstance()->FlushEFBPokes();

  // MSAA case where we need to resolve first.
  // TODO: Do in one pass.
  TargetRectangle scaled_src_rect = g_renderer->ConvertEFBRectangle(src_rect);
  VkRect2D region = {{scaled_src_rect.left, scaled_src_rect.top},
                     {static_cast<u32>(scaled_src_rect.GetWidth()),
                      static_cast<u32>(scaled_src_rect.GetHeight())}};
  Texture2D* src_texture;
  if (is_depth_copy)
    src_texture = FramebufferManager::GetInstance()->ResolveEFBDepthTexture(region);
  else
    src_texture = FramebufferManager::GetInstance()->ResolveEFBColorTexture(region);

  // End render pass before barrier (since we have no self-dependencies).
  // The barrier has to happen after the render pass, not inside it, as we are going to be
  // reading from the texture immediately afterwards.
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->OnReadback();

  // Transition to shader resource before reading.
  VkImageLayout original_layout = src_texture->GetLayout();
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  m_texture_converter->EncodeTextureToMemory(src_texture->GetView(), dst, format, native_width,
                                             bytes_per_row, num_blocks_y, memory_stride,
                                             is_depth_copy, is_intensity, scale_by_half, src_rect);

  // Transition back to original state
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(), original_layout);
}

void TextureCache::CopyRectangleFromTexture(TCacheEntry* dst_texture,
                                            const MathUtil::Rectangle<int>& dst_rect,
                                            Texture2D* src_texture,
                                            const MathUtil::Rectangle<int>& src_rect)
{
  // Fast path when not scaling the image.
  if (src_rect.GetWidth() == dst_rect.GetWidth() && src_rect.GetHeight() == dst_rect.GetHeight())
    CopyTextureRectangle(dst_texture, dst_rect, src_texture, src_rect);
  else
    ScaleTextureRectangle(dst_texture, dst_rect, src_texture, src_rect);
}

bool TextureCache::SupportsGPUTextureDecode(TextureFormat format, TlutFormat palette_format)
{
  return m_texture_converter->SupportsTextureDecoding(format, palette_format);
}

void TextureCache::DecodeTextureOnGPU(TCacheEntryBase* entry, u32 dst_level, const u8* data,
                                      size_t data_size, TextureFormat format, u32 width, u32 height,
                                      u32 aligned_width, u32 aligned_height, u32 row_stride,
                                      const u8* palette, TlutFormat palette_format)
{
  m_texture_converter->DecodeTexture(static_cast<TCacheEntry*>(entry), dst_level, data, data_size,
                                     format, width, height, aligned_width, aligned_height,
                                     row_stride, palette, palette_format);
}

void TextureCache::CopyTextureRectangle(TCacheEntry* dst_texture,
                                        const MathUtil::Rectangle<int>& dst_rect,
                                        Texture2D* src_texture,
                                        const MathUtil::Rectangle<int>& src_rect)
{
  _assert_msg_(VIDEO, static_cast<u32>(src_rect.GetWidth()) <= src_texture->GetWidth() &&
                          static_cast<u32>(src_rect.GetHeight()) <= src_texture->GetHeight(),
               "Source rect is too large for CopyRectangleFromTexture");

  _assert_msg_(VIDEO, static_cast<u32>(dst_rect.GetWidth()) <= dst_texture->config.width &&
                          static_cast<u32>(dst_rect.GetHeight()) <= dst_texture->config.height,
               "Dest rect is too large for CopyRectangleFromTexture");

  VkImageCopy image_copy = {
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
       src_texture->GetLayers()},        // VkImageSubresourceLayers    srcSubresource
      {src_rect.left, src_rect.top, 0},  // VkOffset3D                  srcOffset
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,  // VkImageSubresourceLayers    dstSubresource
       dst_texture->config.layers},
      {dst_rect.left, dst_rect.top, 0},  // VkOffset3D                  dstOffset
      {static_cast<uint32_t>(src_rect.GetWidth()), static_cast<uint32_t>(src_rect.GetHeight()),
       1}  // VkExtent3D                  extent
  };

  // Must be called outside of a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  dst_texture->GetTexture()->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkCmdCopyImage(g_command_buffer_mgr->GetCurrentCommandBuffer(), src_texture->GetImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_texture->GetTexture()->GetImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);
}

void TextureCache::ScaleTextureRectangle(TCacheEntry* dst_texture,
                                         const MathUtil::Rectangle<int>& dst_rect,
                                         Texture2D* src_texture,
                                         const MathUtil::Rectangle<int>& src_rect)
{
  // Can't do this within a game render pass.
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->SetPendingRebind();

  // Can't render to a non-rendertarget (no framebuffer).
  _assert_msg_(VIDEO, dst_texture->config.rendertarget,
               "Destination texture for partial copy is not a rendertarget");

  // Render pass expects dst_texture to be in SHADER_READ_ONLY state.
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  dst_texture->GetTexture()->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD), m_render_pass,
                         g_object_cache->GetPassthroughVertexShader(),
                         g_object_cache->GetPassthroughGeometryShader(), m_copy_shader);

  VkRect2D region = {
      {dst_rect.left, dst_rect.top},
      {static_cast<u32>(dst_rect.GetWidth()), static_cast<u32>(dst_rect.GetHeight())}};
  draw.BeginRenderPass(dst_texture->GetFramebuffer(), region);
  draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetLinearSampler());
  draw.DrawQuad(dst_rect.left, dst_rect.top, dst_rect.GetWidth(), dst_rect.GetHeight(),
                src_rect.left, src_rect.top, 0, src_rect.GetWidth(), src_rect.GetHeight(),
                static_cast<int>(src_texture->GetWidth()),
                static_cast<int>(src_texture->GetHeight()));
  draw.EndRenderPass();
}

TextureCacheBase::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
  // Determine image usage, we need to flag as an attachment if it can be used as a rendertarget.
  VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT;
  if (config.rendertarget)
    usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  // Allocate texture object
  std::unique_ptr<Texture2D> texture = Texture2D::Create(
      config.width, config.height, config.levels, config.layers, TEXTURECACHE_TEXTURE_FORMAT,
      VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL, usage);

  if (!texture)
    return nullptr;

  // If this is a render target (for efb copies), allocate a framebuffer
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  if (config.rendertarget)
  {
    VkImageView framebuffer_attachments[] = {texture->GetView()};
    VkFramebufferCreateInfo framebuffer_info = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        nullptr,
        0,
        m_render_pass,
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
    VkImageSubresourceRange clear_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, config.levels, 0,
                                           config.layers};
    texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdClearColorImage(g_command_buffer_mgr->GetCurrentInitCommandBuffer(), texture->GetImage(),
                         texture->GetLayout(), &clear_value, 1, &clear_range);
  }

  return new TCacheEntry(config, std::move(texture), framebuffer);
}

bool TextureCache::CreateRenderPasses()
{
  static constexpr VkAttachmentDescription update_attachment = {
      0,
      TEXTURECACHE_TEXTURE_FORMAT,
      VK_SAMPLE_COUNT_1_BIT,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_STORE,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_DONT_CARE,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  static constexpr VkAttachmentReference color_attachment_reference = {
      0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  static constexpr VkSubpassDescription subpass_description = {
      0,       VK_PIPELINE_BIND_POINT_GRAPHICS,
      0,       nullptr,
      1,       &color_attachment_reference,
      nullptr, nullptr,
      0,       nullptr};

  VkRenderPassCreateInfo update_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        nullptr,
                                        0,
                                        1,
                                        &update_attachment,
                                        1,
                                        &subpass_description,
                                        0,
                                        nullptr};

  VkResult res =
      vkCreateRenderPass(g_vulkan_context->GetDevice(), &update_info, nullptr, &m_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return false;
  }

  return true;
}

TextureCache::TCacheEntry::TCacheEntry(const TCacheEntryConfig& config_,
                                       std::unique_ptr<Texture2D> texture,
                                       VkFramebuffer framebuffer)
    : TCacheEntryBase(config_), m_texture(std::move(texture)), m_framebuffer(framebuffer)
{
}

TextureCache::TCacheEntry::~TCacheEntry()
{
  // Texture is automatically cleaned up, however, we don't want to leave it bound.
  StateTracker::GetInstance()->UnbindTexture(m_texture->GetView());
  if (m_framebuffer != VK_NULL_HANDLE)
    g_command_buffer_mgr->DeferFramebufferDestruction(m_framebuffer);
}

void TextureCache::TCacheEntry::Load(const u8* buffer, unsigned int width, unsigned int height,
                                     unsigned int expanded_width, unsigned int level)
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
  // This is so that the remaining mip levels can be uploaded without barriers, and then when the
  // texture is used, it can be transitioned to SHADER_READ_ONLY (see TCacheEntry::Bind).
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // Does this texture data fit within the streaming buffer?
  u32 upload_width = width;
  u32 upload_pitch = upload_width * sizeof(u32);
  u32 upload_size = upload_pitch * height;
  u32 upload_alignment = static_cast<u32>(g_vulkan_context->GetBufferImageGranularity());
  u32 source_pitch = expanded_width * 4;
  if ((upload_size + upload_alignment) <= STAGING_TEXTURE_UPLOAD_THRESHOLD &&
      (upload_size + upload_alignment) <= MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE)
  {
    // Assume tightly packed rows, with no padding as the buffer source.
    StreamBuffer* upload_buffer = TextureCache::GetInstance()->m_texture_upload_buffer.get();

    // Allocate memory from the streaming buffer for the texture data.
    if (!upload_buffer->ReserveMemory(upload_size, g_vulkan_context->GetBufferImageGranularity()))
    {
      // Execute the command buffer first.
      WARN_LOG(VIDEO, "Executing command list while waiting for space in texture upload buffer");
      Util::ExecuteCurrentCommandsAndRestoreState(false);

      // Try allocating again. This may cause a fence wait.
      if (!upload_buffer->ReserveMemory(upload_size, g_vulkan_context->GetBufferImageGranularity()))
        PanicAlert("Failed to allocate space in texture upload buffer");
    }

    // Grab buffer pointers
    VkBuffer image_upload_buffer = upload_buffer->GetBuffer();
    VkDeviceSize image_upload_buffer_offset = upload_buffer->GetCurrentOffset();
    u8* image_upload_buffer_pointer = upload_buffer->GetCurrentHostPointer();

    // Copy to the buffer using the stride from the subresource layout
    const u8* source_ptr = buffer;
    if (upload_pitch != source_pitch)
    {
      VkDeviceSize copy_pitch = std::min(source_pitch, upload_pitch);
      for (unsigned int row = 0; row < height; row++)
      {
        memcpy(image_upload_buffer_pointer + row * upload_pitch, source_ptr + row * source_pitch,
               copy_pitch);
      }
    }
    else
    {
      // Can copy the whole thing in one block, the pitch matches
      memcpy(image_upload_buffer_pointer, source_ptr, upload_size);
    }

    // Flush buffer memory if necessary
    upload_buffer->CommitMemory(upload_size);

    // Copy from the streaming buffer to the actual image.
    VkBufferImageCopy image_copy = {
        image_upload_buffer_offset,                // VkDeviceSize                bufferOffset
        0,                                         // uint32_t                    bufferRowLength
        0,                                         // uint32_t                    bufferImageHeight
        {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1},  // VkImageSubresourceLayers    imageSubresource
        {0, 0, 0},                                 // VkOffset3D                  imageOffset
        {width, height, 1}                         // VkExtent3D                  imageExtent
    };
    vkCmdCopyBufferToImage(g_command_buffer_mgr->GetCurrentInitCommandBuffer(), image_upload_buffer,
                           m_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &image_copy);
  }
  else
  {
    // Slow path. The data for the image is too large to fit in the streaming buffer, so we need
    // to allocate a temporary texture to store the data in, then copy to the real texture.
    std::unique_ptr<StagingTexture2D> staging_texture = StagingTexture2D::Create(
        STAGING_BUFFER_TYPE_UPLOAD, width, height, TEXTURECACHE_TEXTURE_FORMAT);

    if (!staging_texture || !staging_texture->Map())
    {
      PanicAlert("Failed to allocate staging texture for large texture upload.");
      return;
    }

    // Copy data to staging texture first, then to the "real" texture.
    staging_texture->WriteTexels(0, 0, width, height, buffer, source_pitch);
    staging_texture->CopyToImage(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                 m_texture->GetImage(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, width,
                                 height, level, 0);
  }
}

void TextureCache::TCacheEntry::FromRenderTarget(bool is_depth_copy, const EFBRectangle& src_rect,
                                                 bool scale_by_half, unsigned int cbufid,
                                                 const float* colmat)
{
  // A better way of doing this would be nice.
  FramebufferManager* framebuffer_mgr =
      static_cast<FramebufferManager*>(g_framebuffer_manager.get());
  TargetRectangle scaled_src_rect = g_renderer->ConvertEFBRectangle(src_rect);

  // Flush EFB pokes first, as they're expected to be included.
  framebuffer_mgr->FlushEFBPokes();

  // Has to be flagged as a render target.
  _assert_(m_framebuffer != VK_NULL_HANDLE);

  // Can't be done in a render pass, since we're doing our own render pass!
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
  StateTracker::GetInstance()->EndRenderPass();

  // Transition EFB to shader resource before binding
  VkRect2D region = {{scaled_src_rect.left, scaled_src_rect.top},
                     {static_cast<u32>(scaled_src_rect.GetWidth()),
                      static_cast<u32>(scaled_src_rect.GetHeight())}};
  Texture2D* src_texture;
  if (is_depth_copy)
    src_texture = FramebufferManager::GetInstance()->ResolveEFBDepthTexture(region);
  else
    src_texture = FramebufferManager::GetInstance()->ResolveEFBColorTexture(region);

  VkSampler src_sampler =
      scale_by_half ? g_object_cache->GetLinearSampler() : g_object_cache->GetPointSampler();
  VkImageLayout original_layout = src_texture->GetLayout();
  src_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  UtilityShaderDraw draw(
      command_buffer, g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_PUSH_CONSTANT),
      TextureCache::GetInstance()->m_render_pass, g_object_cache->GetPassthroughVertexShader(),
      g_object_cache->GetPassthroughGeometryShader(),
      is_depth_copy ? TextureCache::GetInstance()->m_efb_depth_to_tex_shader :
                      TextureCache::GetInstance()->m_efb_color_to_tex_shader);

  draw.SetPushConstants(colmat, (is_depth_copy ? sizeof(float) * 20 : sizeof(float) * 28));
  draw.SetPSSampler(0, src_texture->GetView(), src_sampler);

  VkRect2D dest_region = {{0, 0}, {m_texture->GetWidth(), m_texture->GetHeight()}};

  draw.BeginRenderPass(m_framebuffer, dest_region);

  draw.DrawQuad(0, 0, config.width, config.height, scaled_src_rect.left, scaled_src_rect.top, 0,
                scaled_src_rect.GetWidth(), scaled_src_rect.GetHeight(),
                framebuffer_mgr->GetEFBWidth(), framebuffer_mgr->GetEFBHeight());

  draw.EndRenderPass();

  // We touched everything, so put it back.
  StateTracker::GetInstance()->SetPendingRebind();

  // Transition the EFB back to its original layout.
  src_texture->TransitionToLayout(command_buffer, original_layout);

  // Render pass transitions texture to SHADER_READ_ONLY.
  m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(const TCacheEntryBase* source,
                                                         const MathUtil::Rectangle<int>& src_rect,
                                                         const MathUtil::Rectangle<int>& dst_rect)
{
  const TCacheEntry* source_vk = static_cast<const TCacheEntry*>(source);
  TextureCache::GetInstance()->CopyRectangleFromTexture(this, dst_rect, source_vk->GetTexture(),
                                                        src_rect);

  // Ensure textures are ready for use again.
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  source_vk->GetTexture()->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  StateTracker::GetInstance()->SetTexture(stage, m_texture->GetView());
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, unsigned int level)
{
  _assert_(level < config.levels);

  // Determine dimensions of image we want to save.
  u32 level_width = std::max(1u, config.width >> level);
  u32 level_height = std::max(1u, config.height >> level);

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
                             staging_texture->GetRowStride(), filename, level_width, level_height);

  staging_texture->Unmap();
  return result;
}

bool TextureCache::CompileShaders()
{
  static const char COPY_SHADER_SOURCE[] = R"(
    layout(set = 1, binding = 0) uniform sampler2DArray samp0;

    layout(location = 0) in float3 uv0;
    layout(location = 1) in float4 col0;
    layout(location = 0) out float4 ocol0;

    void main()
    {
      ocol0 = texture(samp0, uv0);
    }
  )";

  static const char EFB_COLOR_TO_TEX_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DArray samp0;

    layout(std140, push_constant) uniform PSBlock
    {
      vec4 colmat[7];
    } C;

    layout(location = 0) in vec3 uv0;
    layout(location = 1) in vec4 col0;
    layout(location = 0) out vec4 ocol0;

    void main()
    {
      float4 texcol = texture(samp0, uv0);
      texcol = floor(texcol * C.colmat[5]) * C.colmat[6];
      ocol0 = texcol * mat4(C.colmat[0], C.colmat[1], C.colmat[2], C.colmat[3]) + C.colmat[4];
    }
  )";

  static const char EFB_DEPTH_TO_TEX_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DArray samp0;

    layout(std140, push_constant) uniform PSBlock
    {
      vec4 colmat[5];
    } C;

    layout(location = 0) in vec3 uv0;
    layout(location = 1) in vec4 col0;
    layout(location = 0) out vec4 ocol0;

    void main()
    {
      #if MONO_DEPTH
        vec4 texcol = texture(samp0, vec3(uv0.xy, 0.0f));
      #else
        vec4 texcol = texture(samp0, uv0);
      #endif
      int depth = int((1.0 - texcol.x) * 16777216.0);

      // Convert to Z24 format
      ivec4 workspace;
      workspace.r = (depth >> 16) & 255;
      workspace.g = (depth >> 8) & 255;
      workspace.b = depth & 255;

      // Convert to Z4 format
      workspace.a = (depth >> 16) & 0xF0;

      // Normalize components to [0.0..1.0]
      texcol = vec4(workspace) / 255.0;

      ocol0 = texcol * mat4(C.colmat[0], C.colmat[1], C.colmat[2], C.colmat[3]) + C.colmat[4];
    }
  )";

  std::string header = g_object_cache->GetUtilityShaderHeader();
  std::string source;

  source = header + COPY_SHADER_SOURCE;
  m_copy_shader = Util::CompileAndCreateFragmentShader(source);

  source = header + EFB_COLOR_TO_TEX_SOURCE;
  m_efb_color_to_tex_shader = Util::CompileAndCreateFragmentShader(source);

  if (g_ActiveConfig.bStereoEFBMonoDepth)
    source = header + "#define MONO_DEPTH 1\n" + EFB_DEPTH_TO_TEX_SOURCE;
  else
    source = header + EFB_DEPTH_TO_TEX_SOURCE;
  m_efb_depth_to_tex_shader = Util::CompileAndCreateFragmentShader(source);

  return m_copy_shader != VK_NULL_HANDLE && m_efb_color_to_tex_shader != VK_NULL_HANDLE &&
         m_efb_depth_to_tex_shader != VK_NULL_HANDLE;
}

void TextureCache::DeleteShaders()
{
  // It is safe to destroy shader modules after they are consumed by creating a pipeline.
  // Therefore, no matter where this function is called from, it won't cause an issue due to
  // pending commands, although at the time of writing should only be called at the end of
  // a frame. See Vulkan spec, section 2.3.1. Object Lifetime.
  if (m_copy_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_copy_shader, nullptr);
    m_copy_shader = VK_NULL_HANDLE;
  }
  if (m_efb_color_to_tex_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_efb_color_to_tex_shader, nullptr);
    m_efb_color_to_tex_shader = VK_NULL_HANDLE;
  }
  if (m_efb_depth_to_tex_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_efb_depth_to_tex_shader, nullptr);
    m_efb_depth_to_tex_shader = VK_NULL_HANDLE;
  }
}

}  // namespace Vulkan
