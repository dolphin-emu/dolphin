// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

#include "VideoCommon/ImageWrite.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PaletteTextureConverter.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/TextureEncoder.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
TextureCache::TextureCache(StateTracker* state_tracker) : m_state_tracker(state_tracker)
{
  m_texture_upload_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, INITIAL_TEXTURE_UPLOAD_BUFFER_SIZE,
                           MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE);
  if (!m_texture_upload_buffer)
    PanicAlert("Failed to create texture upload buffer");

  if (!CreateRenderPasses())
    PanicAlert("Failed to create copy render pass");

  m_texture_encoder = std::make_unique<TextureEncoder>();

  m_palette_texture_converter = std::make_unique<PaletteTextureConverter>();
  if (!m_palette_texture_converter->Initialize())
    PanicAlert("Failed to initialize palette texture converter");
}

TextureCache::~TextureCache()
{
  if (m_image_download_buffer != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(g_object_cache->GetDevice(), m_image_download_buffer, nullptr);
    vkFreeMemory(g_object_cache->GetDevice(), m_image_download_buffer_memory, nullptr);
  }

  if (m_overwrite_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_object_cache->GetDevice(), m_overwrite_render_pass, nullptr);
}

void TextureCache::CompileShaders()
{
}

void TextureCache::DeleteShaders()
{
}

void TextureCache::ConvertTexture(TCacheEntryBase* base_entry, TCacheEntryBase* base_unconverted,
                                  void* palette, TlutFormat format)
{
  TCacheEntry* entry = static_cast<TCacheEntry*>(base_entry);
  TCacheEntry* unconverted = static_cast<TCacheEntry*>(base_unconverted);
  assert(entry->config.rendertarget);

  m_palette_texture_converter->ConvertTexture(
      m_state_tracker, entry->GetTexture(), entry->GetFramebuffer(), unconverted->GetTexture(),
      entry->config.width, entry->config.height, palette, format);
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row,
                           u32 num_blocks_y, u32 memory_stride, PEControl::PixelFormat src_format,
                           const EFBRectangle& src_rect, bool is_intensity, bool scale_by_half)
{
  // A better way of doing this would be nice.
  FramebufferManager* framebuffer_mgr =
      static_cast<FramebufferManager*>(g_framebuffer_manager.get());

  // TODO: MSAA case where we need to resolve first.
  Texture2D* src_texture = (src_format == PEControl::Z24) ? framebuffer_mgr->GetEFBDepthTexture() :
                                                            framebuffer_mgr->GetEFBColorTexture();

  // End render pass before barrier (since we have no self-dependencies)
  m_state_tracker->EndRenderPass();
  m_state_tracker->SetPendingRebind();
  m_state_tracker->InvalidateDescriptorSets();

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
      VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL, usage);

  if (!texture)
    return nullptr;

  // If this is a render target (For efb copies), allocate a framebuffer
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  if (config.rendertarget)
  {
    VkImageView framebuffer_attachments[] = {texture->GetView()};
    VkFramebufferCreateInfo framebuffer_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                nullptr,
                                                0,
                                                m_overwrite_render_pass,
                                                ARRAYSIZE(framebuffer_attachments),
                                                framebuffer_attachments,
                                                texture->GetWidth(),
                                                texture->GetHeight(),
                                                texture->GetLayers()};

    VkResult res =
        vkCreateFramebuffer(g_object_cache->GetDevice(), &framebuffer_info, nullptr, &framebuffer);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
      return nullptr;
    }
  }

  return new TCacheEntry(config, this, std::move(texture), framebuffer);
}

bool TextureCache::CreateRenderPasses()
{
  VkAttachmentDescription attachments[] = {
      {0, TEXTURECACHE_TEXTURE_FORMAT,
       VK_SAMPLE_COUNT_1_BIT,  // TODO: MSAA
       VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE,
       VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkAttachmentReference color_attachment_references[] = {
      {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription subpass_descriptions[] = {{0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1,
                                                  color_attachment_references, nullptr, nullptr, 0,
                                                  nullptr}};

  VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                      nullptr,
                                      0,
                                      ARRAYSIZE(attachments),
                                      attachments,
                                      ARRAYSIZE(subpass_descriptions),
                                      subpass_descriptions,
                                      0,
                                      nullptr};

  VkResult res = vkCreateRenderPass(g_object_cache->GetDevice(), &pass_info, nullptr,
                                    &m_overwrite_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return false;
  }

  return true;
}

bool TextureCache::ResizeImageDownloadBuffer(VkDeviceSize new_size)
{
  if (new_size <= m_image_download_buffer_size)
    return true;

  VkBufferCreateInfo buffer_create_info = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,  // VkStructureType        sType
      nullptr,                               // const void*            pNext
      0,                                     // VkBufferCreateFlags    flags
      new_size,                              // VkDeviceSize           size
      VK_BUFFER_USAGE_TRANSFER_DST_BIT,      // VkBufferUsageFlags     usage
      VK_SHARING_MODE_EXCLUSIVE,             // VkSharingMode          sharingMode
      0,                                     // uint32_t               queueFamilyIndexCount
      nullptr                                // const uint32_t*        pQueueFamilyIndices
  };
  VkBuffer buffer;
  VkResult res = vkCreateBuffer(g_object_cache->GetDevice(), &buffer_create_info, nullptr, &buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBuffer failed: ");
    return false;
  }

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(g_object_cache->GetDevice(), buffer, &memory_requirements);

  uint32_t memory_type_index = g_object_cache->GetMemoryType(memory_requirements.memoryTypeBits,
                                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  VkMemoryAllocateInfo memory_allocate_info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,  // VkStructureType    sType
      nullptr,                                 // const void*        pNext
      memory_requirements.size,                // VkDeviceSize       allocationSize
      memory_type_index                        // uint32_t           memoryTypeIndex
  };
  VkDeviceMemory buffer_memory;
  res =
      vkAllocateMemory(g_object_cache->GetDevice(), &memory_allocate_info, nullptr, &buffer_memory);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkAllocateMemory failed: ");
    vkDestroyBuffer(g_object_cache->GetDevice(), buffer, nullptr);
    return false;
  }

  res = vkBindBufferMemory(g_object_cache->GetDevice(), buffer, buffer_memory, 0);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkBindBufferMemory failed: ");
    return false;
  }

  // Replace pointers. Assume the old buffer is no longer in use.
  if (m_image_download_buffer != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(g_object_cache->GetDevice(), m_image_download_buffer, nullptr);
    vkFreeMemory(g_object_cache->GetDevice(), m_image_download_buffer_memory, nullptr);
  }

  m_image_download_buffer = buffer;
  m_image_download_buffer_memory = buffer_memory;
  m_image_download_buffer_size = new_size;
  return true;
}

bool TextureCache::DownloadImage(VkImage image, VkImageAspectFlags aspect, int x, int y, u32 width,
                                 u32 height, u32 level, u32 layer, void* dst_buffer, u32 dst_stride)
{
  // Determine required size for the buffer.
  // When copying to the intermediate buffer, assume tight packing.
  // TODO: Use bufferRowLength to use same stride as dst_stride.
  u32 src_stride = width * sizeof(u32);
  VkDeviceSize required_buffer_size = src_stride * height;
  if (!ResizeImageDownloadBuffer(required_buffer_size))
  {
    ERROR_LOG(VIDEO, "Failed to resize image download buffer");
    return false;
  }

  // Transition buffer to transfer destination.
  Util::BufferMemoryBarrier(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                            m_image_download_buffer, VK_ACCESS_HOST_READ_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_WHOLE_SIZE,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Issue the image->buffer copy.
  VkBufferImageCopy image_copy = {
      0,                          // VkDeviceSize                bufferOffset
      0,                          // uint32_t                    bufferRowLength
      0,                          // uint32_t                    bufferImageHeight
      {aspect, level, layer, 1},  // VkImageSubresourceLayers    imageSubresource
      {x, y, 0},                  // VkOffset3D                  imageOffset
      {width, height, 1}          // VkExtent3D                  imageExtent
  };
  vkCmdCopyImageToBuffer(g_command_buffer_mgr->GetCurrentCommandBuffer(), image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_image_download_buffer, 1,
                         &image_copy);

  // Ensure the write has completed.
  Util::BufferMemoryBarrier(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                            m_image_download_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_ACCESS_HOST_READ_BIT, 0, VK_WHOLE_SIZE,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

  // Submit command buffer and wait until the GPU has signaled the fence.
  g_command_buffer_mgr->ExecuteCommandBuffer(false, true);

  // Map the download buffer into our address space and copy to the final buffer.
  void* source_ptr;
  VkResult res = vkMapMemory(g_object_cache->GetDevice(), m_image_download_buffer_memory, 0,
                             required_buffer_size, 0, &source_ptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkMapMemory failed: ");
    return false;
  }

  if (src_stride == dst_stride)
  {
    memcpy(dst_buffer, source_ptr, src_stride * height);
  }
  else
  {
    u32 copy_size = std::min(src_stride, dst_stride);
    u8* current_source_ptr = reinterpret_cast<u8*>(source_ptr);
    u8* current_dest_ptr = reinterpret_cast<u8*>(dst_buffer);
    for (u32 row = 0; row < height; row++)
    {
      memcpy(current_dest_ptr, current_source_ptr, copy_size);
      current_source_ptr += src_stride;
      current_dest_ptr += dst_stride;
    }
  }

  vkUnmapMemory(g_object_cache->GetDevice(), m_image_download_buffer_memory);
  return true;
}

TextureCache::TCacheEntry::TCacheEntry(const TCacheEntryConfig& _config, TextureCache* parent,
                                       std::unique_ptr<Texture2D> texture,
                                       VkFramebuffer framebuffer)
    : TCacheEntryBase(_config), m_parent(parent), m_texture(std::move(texture)),
      m_framebuffer(framebuffer)
{
}

TextureCache::TCacheEntry::~TCacheEntry()
{
  // Texture is automatically cleaned up, however, we don't want to leave it bound to the state
  // tracker.
  m_parent->m_state_tracker->UnbindTexture(m_texture->GetView());

  if (m_framebuffer != VK_NULL_HANDLE)
    g_command_buffer_mgr->DeferResourceDestruction(m_framebuffer);
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height,
                                     unsigned int expanded_width, unsigned int level)
{
  StreamBuffer* upload_buffer = m_parent->m_texture_upload_buffer.get();

  // Can't copy data larger than the texture extents.
  width = std::max(1u, std::min(width, m_texture->GetWidth() >> level));
  height = std::max(1u, std::min(height, m_texture->GetHeight() >> level));

  // Assume tightly packed rows, with no padding as the buffer source.
  // Specifying larger sizes with bufferRowLength seems to cause issues on Mesa.
  VkDeviceSize upload_width = width;
  VkDeviceSize upload_pitch = upload_width * sizeof(u32);
  VkDeviceSize upload_size = upload_pitch * height;

  // Allocate memory from the streaming buffer for the texture data.
  // TODO: Handle cases where the texture does not fit into the streaming buffer, we need to
  // allocate a temporary buffer.
  if (!upload_buffer->ReserveMemory(upload_size, g_object_cache->GetBufferImageGranularity()))
  {
    // Execute the command buffer first.
    WARN_LOG(VIDEO, "Executing command list while waiting for space in texture upload buffer");
    Util::ExecuteCurrentCommandsAndRestoreState(m_parent->m_state_tracker, false);

    // Try allocating again. This may cause a fence wait.
    if (!upload_buffer->ReserveMemory(upload_size, g_object_cache->GetBufferImageGranularity()))
      PanicAlert("Failed to allocate space in texture upload buffer");
  }

  // Grab buffer pointers
  VkBuffer image_upload_buffer = upload_buffer->GetBuffer();
  VkDeviceSize image_upload_buffer_offset = upload_buffer->GetCurrentOffset();
  u8* image_upload_buffer_pointer = upload_buffer->GetCurrentHostPointer();

  // Copy to the buffer using the stride from the subresource layout
  VkDeviceSize source_pitch = expanded_width * 4;
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

  // We can't execute texture uploads inside a render pass.
  m_parent->m_state_tracker->EndRenderPass();

  // Intel Mesa hangs with drawing commands plus buffer->image copies in the same command buffer.
  // I suspect that the issue is elsewhere and this just prevents it from happening though.
  // Util::ExecuteCurrentCommandsAndRestoreState(m_parent->m_state_tracker, false);

  // Transition the texture to a transfer destination, invoke the transfer, then transition back.
  // TODO: Only transition the layer we're copying to.
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vkCmdCopyBufferToImage(g_command_buffer_mgr->GetCurrentCommandBuffer(), image_upload_buffer,
                         m_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                         &image_copy);
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dst, PEControl::PixelFormat src_format,
                                                 const EFBRectangle& src_rect, bool scale_by_half,
                                                 unsigned int cbufid, const float* colmat)
{
  // A better way of doing this would be nice.
  FramebufferManager* framebuffer_mgr =
      static_cast<FramebufferManager*>(g_framebuffer_manager.get());
  TargetRectangle scaled_src_rect = g_renderer->ConvertEFBRectangle(src_rect);
  bool is_depth_copy = (src_format == PEControl::Z24);

  // Has to be flagged as a render target.
  assert(m_framebuffer != VK_NULL_HANDLE);

  // Can't be done in a render pass, since we're doing our own render pass!
  m_parent->m_state_tracker->EndRenderPass();

  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();

  // Transition EFB to shader resource before binding
  Texture2D* src_texture =
      is_depth_copy ? framebuffer_mgr->GetEFBDepthTexture() : framebuffer_mgr->GetEFBColorTexture();
  VkSampler src_sampler =
      scale_by_half ? g_object_cache->GetLinearSampler() : g_object_cache->GetPointSampler();
  VkImageLayout original_layout = src_texture->GetLayout();
  src_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  UtilityShaderDraw draw(
      g_object_cache->GetStandardPipelineLayout(), m_parent->m_overwrite_render_pass,
      g_object_cache->GetSharedShaderCache().GetPassthroughVertexShader(),
      g_object_cache->GetSharedShaderCache().GetPassthroughGeometryShader(),
      is_depth_copy ? g_object_cache->GetSharedShaderCache().GetDepthMatrixFragmentShader() :
                      g_object_cache->GetSharedShaderCache().GetColorMatrixFragmentShader());

  // TODO: Hmm. Push constants would be useful here.
  u8* uniform_buffer = draw.AllocatePSUniforms(sizeof(float) * 28);
  memcpy(uniform_buffer, colmat, (is_depth_copy ? sizeof(float) * 20 : sizeof(float) * 28));
  draw.CommitPSUniforms(sizeof(float) * 28);

  draw.SetPSSampler(0, src_texture->GetView(), src_sampler);

  VkRect2D dest_region = {{0, 0}, {m_texture->GetWidth(), m_texture->GetHeight()}};

  draw.BeginRenderPass(m_framebuffer, dest_region);

  draw.DrawQuad(scaled_src_rect.left, scaled_src_rect.top, scaled_src_rect.GetWidth(),
                scaled_src_rect.GetHeight(), framebuffer_mgr->GetEFBWidth(),
                framebuffer_mgr->GetEFBHeight(), 0, 0, config.width, config.height);

  draw.EndRenderPass();

  // We touched everything, so put it back.
  m_parent->m_state_tracker->SetPendingRebind();

  // Transition the destination texture to shader resource, and the EFB to its original layout.
  m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  src_texture->TransitionToLayout(command_buffer, original_layout);
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(const TCacheEntryBase* source,
                                                         const MathUtil::Rectangle<int>& src_rect,
                                                         const MathUtil::Rectangle<int>& dst_rect)
{
  const TCacheEntry* source_vk = static_cast<const TCacheEntry*>(source);
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();

  // Fast path when not scaling the image.
  if (src_rect.GetWidth() == dst_rect.GetWidth() && src_rect.GetHeight() == dst_rect.GetHeight())
  {
    // These assertions should hold true unless the base code is passing us sizes too large, in
    // which case it should be fixed instead.
    _assert_msg_(VIDEO, static_cast<u32>(src_rect.GetWidth()) <= source->config.width &&
                            static_cast<u32>(src_rect.GetHeight()) <= source->config.height,
                 "Source rect is too large for CopyRectangleFromTexture");

    _assert_msg_(VIDEO, static_cast<u32>(dst_rect.GetWidth()) <= config.width &&
                            static_cast<u32>(dst_rect.GetHeight()) <= config.height,
                 "Dest rect is too large for CopyRectangleFromTexture");

    VkImageCopy image_copy = {
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
         source->config.layers},           // VkImageSubresourceLayers    srcSubresource
        {src_rect.left, src_rect.top, 0},  // VkOffset3D                  srcOffset
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
         config.layers},                   // VkImageSubresourceLayers    dstSubresource
        {dst_rect.left, dst_rect.top, 0},  // VkOffset3D                  dstOffset
        {static_cast<uint32_t>(src_rect.GetWidth()),
         static_cast<uint32_t>(src_rect.GetHeight())}  // VkExtent3D                  extent
    };

    // Must be called outside of a render pass.
    m_parent->m_state_tracker->EndRenderPass();

    source_vk->m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdCopyImage(command_buffer, source_vk->m_texture->GetImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_texture->GetImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

    m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    source_vk->m_texture->TransitionToLayout(command_buffer,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return;
  }

  // Can't do this within a game render pass.
  m_parent->m_state_tracker->EndRenderPass();
  m_parent->m_state_tracker->SetPendingRebind();

  // Can't render to a non-rendertarget (no framebuffer).
  _assert_msg_(VIDEO, config.rendertarget,
               "Destination texture for partial copy is not a rendertarget");

  // Transition resource states. Source should already be in shader resource layout.
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  UtilityShaderDraw draw(
      g_object_cache->GetStandardPipelineLayout(), m_parent->m_overwrite_render_pass,
      g_object_cache->GetSharedShaderCache().GetPassthroughVertexShader(), VK_NULL_HANDLE,
      g_object_cache->GetSharedShaderCache().GetCopyFragmentShader());

  VkRect2D region = {
      {dst_rect.left, dst_rect.top},
      {static_cast<u32>(dst_rect.GetWidth()), static_cast<u32>(dst_rect.GetHeight())}};
  draw.BeginRenderPass(m_framebuffer, region);
  draw.SetPSSampler(0, source_vk->GetTexture()->GetView(), g_object_cache->GetLinearSampler());
  draw.DrawQuad(src_rect.left, src_rect.top, src_rect.GetWidth(), src_rect.GetHeight(),
                source->config.width, source->config.height, dst_rect.left, dst_rect.top,
                dst_rect.GetWidth(), dst_rect.GetHeight());
  draw.EndRenderPass();

  // Transition back to shader resource.
  // TODO: Move this to the render pass.
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
  m_parent->m_state_tracker->SetTexture(stage, m_texture->GetView());
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, unsigned int level)
{
  _assert_(level < config.levels);

  // Determine dimensions of image we want to save.
  u32 level_width = std::max(1u, config.width >> level);
  u32 level_height = std::max(1u, config.height >> level);

  // Allocate a buffer for storing this image.
  // This is sub-optimal, since we could write the buffer returned by vkMapMemory
  // directly out to the image, but this is a slow path anyway.
  u32 buffer_stride = level_width * sizeof(u32);
  std::vector<u8> buffer(buffer_stride * level_height);

  // Transition image to transfer source, and invalidate the current state,
  // since we'll be executing the command buffer.
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_parent->m_state_tracker->EndRenderPass();
  m_parent->m_state_tracker->SetPendingRebind();
  m_parent->m_state_tracker->InvalidateDescriptorSets();

  // Copy to download buffer.
  if (!m_parent->DownloadImage(m_texture->GetImage(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, level_width,
                               level_height, level, 0, buffer.data(), buffer_stride))
  {
    ERROR_LOG(VIDEO, "Failed to download image to buffer");
    return false;
  }

  // Restore original state of texture.
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Write texture out to file.
  return TextureToPng(buffer.data(), buffer_stride, filename, level_width, level_height);
}

}  // namespace Vulkan
