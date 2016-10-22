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
#include "VideoBackends/Vulkan/PaletteTextureConverter.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/TextureEncoder.h"
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
  if (m_initialize_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_initialize_render_pass, nullptr);
  if (m_update_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_update_render_pass, nullptr);
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

  m_texture_encoder = std::make_unique<TextureEncoder>();
  if (!m_texture_encoder->Initialize())
  {
    PanicAlert("Failed to initialize texture encoder.");
    return false;
  }

  m_palette_texture_converter = std::make_unique<PaletteTextureConverter>();
  if (!m_palette_texture_converter->Initialize())
  {
    PanicAlert("Failed to initialize palette texture converter");
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
  _assert_(entry->config.rendertarget);

  // EFB copies can be used as paletted textures as well. For these, we can't assume them to be
  // contain the correct data before the frame begins (when the init command buffer is executed),
  // so we must convert them at the appropriate time, during the drawing command buffer.
  VkCommandBuffer command_buffer;
  if (unconverted->IsEfbCopy())
  {
    command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
    StateTracker::GetInstance()->EndRenderPass();
    StateTracker::GetInstance()->SetPendingRebind();
  }
  else
  {
    // Use initialization command buffer and perform conversion before the drawing commands.
    command_buffer = g_command_buffer_mgr->GetCurrentInitCommandBuffer();
  }

  m_palette_texture_converter->ConvertTexture(
      command_buffer, GetRenderPassForTextureUpdate(entry->GetTexture()), entry->GetFramebuffer(),
      unconverted->GetTexture(), entry->config.width, entry->config.height, palette, format,
      unconverted->format);

  // Render pass transitions to SHADER_READ_ONLY.
  entry->GetTexture()->OverrideImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

static bool IsDepthCopyFormat(PEControl::PixelFormat format)
{
  return format == PEControl::Z24;
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row,
                           u32 num_blocks_y, u32 memory_stride, PEControl::PixelFormat src_format,
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
  if (IsDepthCopyFormat(src_format))
    src_texture = FramebufferManager::GetInstance()->ResolveEFBDepthTexture(region);
  else
    src_texture = FramebufferManager::GetInstance()->ResolveEFBColorTexture(region);

  // End render pass before barrier (since we have no self-dependencies)
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->SetPendingRebind();
  StateTracker::GetInstance()->InvalidateDescriptorSets();
  StateTracker::GetInstance()->OnReadback();

  // Transition to shader resource before reading.
  VkImageLayout original_layout = src_texture->GetLayout();
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  m_texture_encoder->EncodeTextureToRam(src_texture->GetView(), dst, format, native_width,
                                        bytes_per_row, num_blocks_y, memory_stride, src_format,
                                        is_intensity, scale_by_half, src_rect);

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
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetStandardPipelineLayout(),
                         GetRenderPassForTextureUpdate(dst_texture->GetTexture()),
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

  // Render pass transitions destination texture to SHADER_READ_ONLY.
  dst_texture->GetTexture()->OverrideImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
        m_initialize_render_pass,
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
  static constexpr VkAttachmentDescription initialize_attachment = {
      0,
      TEXTURECACHE_TEXTURE_FORMAT,
      VK_SAMPLE_COUNT_1_BIT,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_STORE,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_DONT_CARE,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

  static constexpr VkAttachmentDescription update_attachment = {
      0,
      TEXTURECACHE_TEXTURE_FORMAT,
      VK_SAMPLE_COUNT_1_BIT,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_STORE,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_DONT_CARE,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

  static constexpr VkAttachmentReference color_attachment_reference = {
      0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  static constexpr VkSubpassDescription subpass_description = {
      0,       VK_PIPELINE_BIND_POINT_GRAPHICS,
      0,       nullptr,
      1,       &color_attachment_reference,
      nullptr, nullptr,
      0,       nullptr};

  static constexpr VkSubpassDependency initialize_dependancies[] = {
      {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_TRANSFER_BIT,
       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
       VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
       VK_DEPENDENCY_BY_REGION_BIT},
      {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
       VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
       VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT}};

  static constexpr VkSubpassDependency update_dependancies[] = {
      {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
       VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
       VK_DEPENDENCY_BY_REGION_BIT},
      {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
       VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
       VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT}};

  VkRenderPassCreateInfo initialize_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                            nullptr,
                                            0,
                                            1,
                                            &initialize_attachment,
                                            1,
                                            &subpass_description,
                                            static_cast<u32>(ArraySize(initialize_dependancies)),
                                            initialize_dependancies};

  VkRenderPassCreateInfo update_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        nullptr,
                                        0,
                                        1,
                                        &update_attachment,
                                        1,
                                        &subpass_description,
                                        static_cast<u32>(ArraySize(update_dependancies)),
                                        update_dependancies};

  VkResult res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &initialize_info, nullptr,
                                    &m_initialize_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass (initialize) failed: ");
    return false;
  }

  res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &update_info, nullptr,
                           &m_update_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass (update) failed: ");
    return false;
  }

  return true;
}

VkRenderPass TextureCache::GetRenderPassForTextureUpdate(const Texture2D* texture) const
{
  // EFB copies can be re-used as part of the texture pool. If this is the case, we need to insert
  // a pipeline barrier to ensure that all reads from the texture expecting the old data have
  // completed before overwriting the texture's contents. New textures will be in TRANSFER_DST
  // due to the clear after creation.

  // These two render passes are compatible, so even though the framebuffer was created with
  // the initialize render pass it's still allowed.

  if (texture->GetLayout() == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    return m_initialize_render_pass;
  else
    return m_update_render_pass;
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

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
                                     unsigned int expanded_width, unsigned int level)
{
  // Can't copy data larger than the texture extents.
  width = std::max(1u, std::min(width, m_texture->GetWidth() >> level));
  height = std::max(1u, std::min(height, m_texture->GetHeight() >> level));

  // We don't care about the existing contents of the texture, so we set the image layout to
  // VK_IMAGE_LAYOUT_UNDEFINED here. However, if this texture is being re-used from the texture
  // pool, it may still be in use. We assume that it's not, as non-efb-copy textures are only
  // returned to the pool when the frame number is different, furthermore, we're doing this
  // on the initialize command buffer, so a texture being re-used mid-frame would have undesirable
  // effects regardless.
  VkImageMemoryBarrier barrier = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType            sType
      nullptr,                                 // const void*                pNext
      0,                                       // VkAccessFlags              srcAccessMask
      VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags              dstAccessMask
      VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout              oldLayout
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,    // VkImageLayout              newLayout
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   srcQueueFamilyIndex
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   dstQueueFamilyIndex
      m_texture->GetImage(),                   // VkImage                    image
      {VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1},  // VkImageSubresourceRange    subresourceRange
  };
  vkCmdPipelineBarrier(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

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
    const u8* source_ptr = TextureCache::temp;
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
    staging_texture->WriteTexels(0, 0, width, height, TextureCache::temp, source_pitch);
    staging_texture->CopyToImage(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                 m_texture->GetImage(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, width,
                                 height, level, 0);
  }

  // Transition to shader read only.
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vkCmdPipelineBarrier(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
  m_texture->OverrideImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dst, PEControl::PixelFormat src_format,
                                                 const EFBRectangle& src_rect, bool scale_by_half,
                                                 unsigned int cbufid, const float* colmat)
{
  // A better way of doing this would be nice.
  FramebufferManager* framebuffer_mgr =
      static_cast<FramebufferManager*>(g_framebuffer_manager.get());
  TargetRectangle scaled_src_rect = g_renderer->ConvertEFBRectangle(src_rect);
  bool is_depth_copy = IsDepthCopyFormat(src_format);

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

  UtilityShaderDraw draw(
      command_buffer, g_object_cache->GetPushConstantPipelineLayout(),
      TextureCache::GetInstance()->GetRenderPassForTextureUpdate(m_texture.get()),
      g_object_cache->GetPassthroughVertexShader(), g_object_cache->GetPassthroughGeometryShader(),
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
  m_texture->OverrideImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
  g_command_buffer_mgr->ExecuteCommandBuffer(false, true);
  StateTracker::GetInstance()->InvalidateDescriptorSets();
  StateTracker::GetInstance()->SetPendingRebind();

  // Map the staging texture so we can copy the contents out.
  if (staging_texture->Map())
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
      texcol = round(texcol * C.colmat[5]) * C.colmat[6];
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

  static const char RGB_TO_YUYV_SHADER_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DArray source;
    layout(location = 0) in vec3 uv0;
    layout(location = 0) out vec4 ocol0;

    const vec3 y_const = vec3(0.257,0.504,0.098);
    const vec3 u_const = vec3(-0.148,-0.291,0.439);
    const vec3 v_const = vec3(0.439,-0.368,-0.071);
    const vec4 const3 = vec4(0.0625,0.5,0.0625,0.5);

    void main()
    {
      vec3 c0 = texture(source, vec3(uv0.xy - dFdx(uv0.xy) * 0.25, 0.0)).rgb;
      vec3 c1 = texture(source, vec3(uv0.xy + dFdx(uv0.xy) * 0.25, 0.0)).rgb;
      vec3 c01 = (c0 + c1) * 0.5;
      ocol0 = vec4(dot(c1, y_const),
                   dot(c01,u_const),
                   dot(c0,y_const),
                   dot(c01, v_const)) + const3;
    }
  )";

  static const char YUYV_TO_RGB_SHADER_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2D source;
    layout(location = 0) in vec3 uv0;
    layout(location = 0) out vec4 ocol0;

    void main()
    {
      ivec2 uv = ivec2(gl_FragCoord.xy);
      vec4 c0 = texelFetch(source, ivec2(uv.x / 2, uv.y), 0);

      // The texture used to stage the upload is in BGRA order.
      c0 = c0.zyxw;

      float y = mix(c0.r, c0.b, (uv.x & 1) == 1);
      float yComp = 1.164 * (y - 0.0625);
      float uComp = c0.g - 0.5;
      float vComp = c0.a - 0.5;
      ocol0 = vec4(yComp + (1.596 * vComp),
                   yComp - (0.813 * vComp) - (0.391 * uComp),
                   yComp + (2.018 * uComp),
                   1.0);
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

  source = header + RGB_TO_YUYV_SHADER_SOURCE;
  m_rgb_to_yuyv_shader = Util::CompileAndCreateFragmentShader(source);
  source = header + YUYV_TO_RGB_SHADER_SOURCE;
  m_yuyv_to_rgb_shader = Util::CompileAndCreateFragmentShader(source);

  return (m_copy_shader != VK_NULL_HANDLE && m_efb_color_to_tex_shader != VK_NULL_HANDLE &&
          m_efb_depth_to_tex_shader != VK_NULL_HANDLE && m_rgb_to_yuyv_shader != VK_NULL_HANDLE &&
          m_yuyv_to_rgb_shader != VK_NULL_HANDLE);
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
  if (m_rgb_to_yuyv_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_rgb_to_yuyv_shader, nullptr);
    m_rgb_to_yuyv_shader = VK_NULL_HANDLE;
  }
  if (m_yuyv_to_rgb_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_yuyv_to_rgb_shader, nullptr);
    m_yuyv_to_rgb_shader = VK_NULL_HANDLE;
  }
}

void TextureCache::EncodeYUYVTextureToMemory(void* dst_ptr, u32 dst_width, u32 dst_stride,
                                             u32 dst_height, Texture2D* src_texture,
                                             const MathUtil::Rectangle<int>& src_rect)
{
  StateTracker::GetInstance()->EndRenderPass();

  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
  src_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Borrow framebuffer from EFB2RAM encoder.
  Texture2D* encoding_texture = m_texture_encoder->GetEncodingTexture();
  StagingTexture2D* download_texture = m_texture_encoder->GetDownloadTexture();
  encoding_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Use fragment shader to convert RGBA to YUYV.
  // Use linear sampler for downscaling. This texture is in BGRA order, so the data is already in
  // the order the guest is expecting and we don't have to swap it at readback time. The width
  // is halved because we're using an RGBA8 texture, but the YUYV data is two bytes per pixel.
  u32 output_width = dst_width / 2;
  UtilityShaderDraw draw(command_buffer, g_object_cache->GetStandardPipelineLayout(),
                         m_texture_encoder->GetEncodingRenderPass(),
                         g_object_cache->GetPassthroughVertexShader(), VK_NULL_HANDLE,
                         m_rgb_to_yuyv_shader);
  VkRect2D region = {{0, 0}, {output_width, dst_height}};
  draw.BeginRenderPass(m_texture_encoder->GetEncodingTextureFramebuffer(), region);
  draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetPointSampler());
  draw.DrawQuad(0, 0, static_cast<int>(output_width), static_cast<int>(dst_height), src_rect.left,
                src_rect.top, 0, src_rect.GetWidth(), src_rect.GetHeight(),
                static_cast<int>(src_texture->GetWidth()),
                static_cast<int>(src_texture->GetHeight()));
  draw.EndRenderPass();

  // Render pass transitions to TRANSFER_SRC.
  encoding_texture->OverrideImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  // Copy from encoding texture to download buffer.
  download_texture->CopyFromImage(command_buffer, encoding_texture->GetImage(),
                                  VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, output_width, dst_height, 0, 0);
  Util::ExecuteCurrentCommandsAndRestoreState(false, true);

  // Finally, copy to guest memory. This may have a different stride.
  download_texture->ReadTexels(0, 0, output_width, dst_height, dst_ptr, dst_stride);
}

void TextureCache::DecodeYUYVTextureFromMemory(TCacheEntry* dst_texture, const void* src_ptr,
                                               u32 src_width, u32 src_stride, u32 src_height)
{
  // Copies (and our decoding step) cannot be done inside a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  // We share the upload buffer with normal textures here, since the XFB buffers aren't very large.
  u32 upload_size = src_stride * src_height;
  if (!m_texture_upload_buffer->ReserveMemory(upload_size,
                                              g_vulkan_context->GetBufferImageGranularity()))
  {
    // Execute the command buffer first.
    WARN_LOG(VIDEO, "Executing command list while waiting for space in texture upload buffer");
    Util::ExecuteCurrentCommandsAndRestoreState(false);
    if (!m_texture_upload_buffer->ReserveMemory(upload_size,
                                                g_vulkan_context->GetBufferImageGranularity()))
      PanicAlert("Failed to allocate space in texture upload buffer");
  }

  // Assume that each source row is not padded.
  _assert_(src_stride == (src_width * sizeof(u16)));
  VkDeviceSize image_upload_buffer_offset = m_texture_upload_buffer->GetCurrentOffset();
  std::memcpy(m_texture_upload_buffer->GetCurrentHostPointer(), src_ptr, upload_size);
  m_texture_upload_buffer->CommitMemory(upload_size);

  // Copy from the upload buffer to the intermediate texture. We borrow this from the encoder.
  // The width is specified as half here because we have two pixels packed in each RGBA texel.
  // In the future this could be skipped by reading the upload buffer as a uniform texel buffer.
  VkBufferImageCopy image_copy = {
      image_upload_buffer_offset,            // VkDeviceSize                bufferOffset
      0,                                     // uint32_t                    bufferRowLength
      0,                                     // uint32_t                    bufferImageHeight
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},  // VkImageSubresourceLayers    imageSubresource
      {0, 0, 0},                             // VkOffset3D                  imageOffset
      {src_width / 2, src_height, 1}         // VkExtent3D                  imageExtent
  };
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
  Texture2D* intermediate_texture = m_texture_encoder->GetEncodingTexture();
  intermediate_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vkCmdCopyBufferToImage(command_buffer, m_texture_upload_buffer->GetBuffer(),
                         intermediate_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                         &image_copy);
  intermediate_texture->TransitionToLayout(command_buffer,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  dst_texture->GetTexture()->TransitionToLayout(command_buffer,
                                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Convert from the YUYV data now in the intermediate texture to RGBA in the destination.
  UtilityShaderDraw draw(command_buffer, g_object_cache->GetStandardPipelineLayout(),
                         m_texture_encoder->GetEncodingRenderPass(),
                         g_object_cache->GetScreenQuadVertexShader(), VK_NULL_HANDLE,
                         m_yuyv_to_rgb_shader);
  VkRect2D region = {{0, 0}, {src_width, src_height}};
  draw.BeginRenderPass(dst_texture->GetFramebuffer(), region);
  draw.SetViewportAndScissor(0, 0, static_cast<int>(src_width), static_cast<int>(src_height));
  draw.SetPSSampler(0, intermediate_texture->GetView(), g_object_cache->GetPointSampler());
  draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
  draw.EndRenderPass();
}

}  // namespace Vulkan
