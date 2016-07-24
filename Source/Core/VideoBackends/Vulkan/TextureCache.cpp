// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

#include "Common/CommonFuncs.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/EFBCache.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PaletteTextureConverter.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/TextureEncoder.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

#include "VideoCommon/ImageWrite.h"

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

  TextureCache::CompileShaders();
}

TextureCache::~TextureCache()
{
  if (m_initialize_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_object_cache->GetDevice(), m_initialize_render_pass, nullptr);
  if (m_update_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_object_cache->GetDevice(), m_update_render_pass, nullptr);
}

void TextureCache::ConvertTexture(TCacheEntryBase* base_entry, TCacheEntryBase* base_unconverted,
                                  void* palette, TlutFormat format)
{
  TCacheEntry* entry = static_cast<TCacheEntry*>(base_entry);
  TCacheEntry* unconverted = static_cast<TCacheEntry*>(base_unconverted);
  assert(entry->config.rendertarget);

  m_palette_texture_converter->ConvertTexture(
      m_state_tracker, GetRenderPassForTextureUpdate(entry->GetTexture()), entry->GetFramebuffer(),
      unconverted->GetTexture(), entry->config.width, entry->config.height, palette, format);

  // Render pass transitions to SHADER_READ_ONLY.
  entry->GetTexture()->OverrideImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row,
                           u32 num_blocks_y, u32 memory_stride, PEControl::PixelFormat src_format,
                           const EFBRectangle& src_rect, bool is_intensity, bool scale_by_half)
{
  // Flush EFB pokes first, as they're expected to be included.
  static_cast<Renderer*>(g_renderer.get())->GetEFBCache()->FlushEFBPokes(m_state_tracker);

  // A better way of doing this would be nice.
  FramebufferManager* framebuffer_mgr =
      static_cast<FramebufferManager*>(g_framebuffer_manager.get());

  // MSAA case where we need to resolve first.
  // TODO: Do in one pass.
  TargetRectangle scaled_src_rect = g_renderer->ConvertEFBRectangle(src_rect);
  VkRect2D region = {{scaled_src_rect.left, scaled_src_rect.top},
                     {static_cast<u32>(scaled_src_rect.GetWidth()),
                      static_cast<u32>(scaled_src_rect.GetHeight())}};
  Texture2D* src_texture = (src_format == PEControl::Z24) ?
                               framebuffer_mgr->ResolveEFBDepthTexture(m_state_tracker, region) :
                               framebuffer_mgr->ResolveEFBColorTexture(m_state_tracker, region);

  // End render pass before barrier (since we have no self-dependencies)
  m_state_tracker->EndRenderPass();
  m_state_tracker->SetPendingRebind();
  m_state_tracker->InvalidateDescriptorSets();
  m_state_tracker->OnReadback();

  // Temporary fix for corruption on Intel/Mesa.
  // g_command_buffer_mgr->ExecuteCommandBuffer(true, false);

  // Transition to shader resource before reading.
  VkImageLayout original_layout = src_texture->GetLayout();
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  m_texture_encoder->EncodeTextureToRam(m_state_tracker, src_texture->GetView(), dst, format,
                                        native_width, bytes_per_row, num_blocks_y, memory_stride,
                                        src_format, is_intensity, scale_by_half, src_rect);

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
      VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL, usage);

  if (!texture)
    return nullptr;

  // If this is a render target (For efb copies), allocate a framebuffer
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

    VkResult res =
        vkCreateFramebuffer(g_object_cache->GetDevice(), &framebuffer_info, nullptr, &framebuffer);
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

  return new TCacheEntry(config, this, std::move(texture), framebuffer);
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

  VkResult res = vkCreateRenderPass(g_object_cache->GetDevice(), &initialize_info, nullptr,
                                    &m_initialize_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass (initialize) failed: ");
    return false;
  }

  res =
      vkCreateRenderPass(g_object_cache->GetDevice(), &update_info, nullptr, &m_update_render_pass);
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

  // We're assuming that Load() is only called for new textures, therefore we can ignore the
  // current contents of the texture (VK_IMAGE_LAYOUT_UNDEFINED).
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
  // Flush EFB pokes first, as they're expected to be included.
  static_cast<Renderer*>(g_renderer.get())->GetEFBCache()->FlushEFBPokes(m_parent->m_state_tracker);

  // A better way of doing this would be nice.
  FramebufferManager* framebuffer_mgr =
      static_cast<FramebufferManager*>(g_framebuffer_manager.get());
  TargetRectangle scaled_src_rect = g_renderer->ConvertEFBRectangle(src_rect);
  bool is_depth_copy = (src_format == PEControl::Z24);

  // Has to be flagged as a render target.
  assert(m_framebuffer != VK_NULL_HANDLE);

  // Can't be done in a render pass, since we're doing our own render pass!
  StateTracker* state_tracker = m_parent->m_state_tracker;
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();
  state_tracker->EndRenderPass();

  // Transition EFB to shader resource before binding
  VkRect2D region = {{scaled_src_rect.left, scaled_src_rect.top},
                     {static_cast<u32>(scaled_src_rect.GetWidth()),
                      static_cast<u32>(scaled_src_rect.GetHeight())}};
  Texture2D* src_texture = is_depth_copy ?
                               framebuffer_mgr->ResolveEFBDepthTexture(state_tracker, region) :
                               framebuffer_mgr->ResolveEFBColorTexture(state_tracker, region);
  VkSampler src_sampler =
      scale_by_half ? g_object_cache->GetLinearSampler() : g_object_cache->GetPointSampler();
  VkImageLayout original_layout = src_texture->GetLayout();
  src_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  UtilityShaderDraw draw(
      command_buffer, g_object_cache->GetPushConstantPipelineLayout(),
      m_parent->GetRenderPassForTextureUpdate(m_texture.get()),
      g_object_cache->GetPassthroughVertexShader(), g_object_cache->GetPassthroughGeometryShader(),
      is_depth_copy ? m_parent->m_efb_depth_to_tex_shader : m_parent->m_efb_color_to_tex_shader);

  draw.SetPushConstants(colmat, (is_depth_copy ? sizeof(float) * 20 : sizeof(float) * 28));
  draw.SetPSSampler(0, src_texture->GetView(), src_sampler);

  VkRect2D dest_region = {{0, 0}, {m_texture->GetWidth(), m_texture->GetHeight()}};

  draw.BeginRenderPass(m_framebuffer, dest_region);

  draw.DrawQuad(0, 0, config.width, config.height, scaled_src_rect.left, scaled_src_rect.top, 0,
                scaled_src_rect.GetWidth(), scaled_src_rect.GetHeight(),
                framebuffer_mgr->GetEFBWidth(), framebuffer_mgr->GetEFBHeight());

  draw.EndRenderPass();

  // We touched everything, so put it back.
  state_tracker->SetPendingRebind();

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
        {static_cast<uint32_t>(src_rect.GetWidth()), static_cast<uint32_t>(src_rect.GetHeight()),
         1}  // VkExtent3D                  extent
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

  UtilityShaderDraw draw(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), g_object_cache->GetStandardPipelineLayout(),
      m_parent->GetRenderPassForTextureUpdate(m_texture.get()),
      g_object_cache->GetPassthroughVertexShader(), VK_NULL_HANDLE, m_parent->m_copy_shader);

  VkRect2D region = {
      {dst_rect.left, dst_rect.top},
      {static_cast<u32>(dst_rect.GetWidth()), static_cast<u32>(dst_rect.GetHeight())}};
  draw.BeginRenderPass(m_framebuffer, region);
  draw.SetPSSampler(0, source_vk->GetTexture()->GetView(), g_object_cache->GetLinearSampler());
  draw.DrawQuad(dst_rect.left, dst_rect.top, dst_rect.GetWidth(), dst_rect.GetHeight(),
                src_rect.left, src_rect.top, 0, src_rect.GetWidth(), src_rect.GetHeight(),
                source->config.width, source->config.height);
  draw.EndRenderPass();

  // Render pass transitions texture to SHADER_READ_ONLY.
  m_texture->OverrideImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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

  // Use a temporary staging texture for the download. Certainly not optimal,
  // but since we have to idle the GPU anyway it doesn't really matter.
  std::unique_ptr<StagingTexture2D> staging_texture =
      StagingTexture2D::Create(level_width, level_height, TEXTURECACHE_TEXTURE_FORMAT);

  // Transition image to transfer source, and invalidate the current state,
  // since we'll be executing the command buffer.
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_parent->m_state_tracker->EndRenderPass();

  // Copy to download buffer.
  staging_texture->CopyFromImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                 m_texture->GetImage(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
                                 level_width, level_height, level, 0);

  // Restore original state of texture.
  m_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Block until the GPU has finished copying to the staging texture.
  g_command_buffer_mgr->ExecuteCommandBuffer(false, true);
  m_parent->m_state_tracker->InvalidateDescriptorSets();
  m_parent->m_state_tracker->SetPendingRebind();

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

void TextureCache::CompileShaders()
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

  std::string header = g_object_cache->GetUtilityShaderHeader();
  std::string source;

  source = header + COPY_SHADER_SOURCE;
  m_copy_shader = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(source);

  source = header + EFB_COLOR_TO_TEX_SOURCE;
  m_efb_color_to_tex_shader = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(source);

  if (g_ActiveConfig.bStereoEFBMonoDepth)
    source = header + "#define MONO_DEPTH 1\n" + EFB_DEPTH_TO_TEX_SOURCE;
  else
    source = header + EFB_DEPTH_TO_TEX_SOURCE;
  m_efb_depth_to_tex_shader = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(source);

  if (m_copy_shader == VK_NULL_HANDLE || m_efb_color_to_tex_shader == VK_NULL_HANDLE ||
      m_efb_depth_to_tex_shader == VK_NULL_HANDLE)
  {
    PanicAlert("Failed to compile one or more shaders");
    return;
  }
}

void TextureCache::DeleteShaders()
{
  auto DestroyShader = [this](VkShaderModule& shader) {
    if (shader != VK_NULL_HANDLE)
    {
      vkDestroyShaderModule(g_object_cache->GetDevice(), shader, nullptr);
      shader = VK_NULL_HANDLE;
    }
  };

  // Since this can be called by the base class we need to wait for idle.
  g_command_buffer_mgr->WaitForGPUIdle();

  DestroyShader(m_copy_shader);
  DestroyShader(m_efb_color_to_tex_shader);
  DestroyShader(m_efb_depth_to_tex_shader);
}

}  // namespace Vulkan
