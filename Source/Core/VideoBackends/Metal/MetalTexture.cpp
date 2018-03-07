// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Metal/MetalTexture.h"
#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/Metal/CommandBufferManager.h"
#include "VideoBackends/Metal/MetalContext.h"
#include "VideoBackends/Metal/MetalFramebuffer.h"
#include "VideoBackends/Metal/Render.h"
#include "VideoBackends/Metal/StateTracker.h"
#include "VideoBackends/Metal/StreamBuffer.h"
#include "VideoBackends/Metal/Util.h"

namespace Metal
{
static mtlpp::TextureDescriptor GetTextureDescriptor(const TextureConfig& cfg)
{
  mtlpp::TextureDescriptor desc;
  desc.SetTextureType(mtlpp::TextureType::Texture2DArray);
  desc.SetWidth(cfg.width);
  desc.SetHeight(cfg.height);
  desc.SetMipmapLevelCount(cfg.levels);
  desc.SetArrayLength(cfg.layers);
  desc.SetPixelFormat(Util::GetMetalPixelFormat(cfg.format));

  u32 usage = static_cast<u32>(mtlpp::TextureUsage::ShaderRead);
  if (cfg.rendertarget)
    usage |= static_cast<u32>(mtlpp::TextureUsage::RenderTarget);

  desc.SetUsage(static_cast<mtlpp::TextureUsage>(usage));
  desc.SetCpuCacheMode(mtlpp::CpuCacheMode::DefaultCache);
  desc.SetStorageMode(mtlpp::StorageMode::Private);
  return desc;
}

MetalTexture::MetalTexture(const TextureConfig& config, mtlpp::Texture tex,
                           std::unique_ptr<MetalFramebuffer> fb)
    : AbstractTexture(config), m_texture(tex), m_framebuffer(std::move(fb))
{
}

MetalTexture::~MetalTexture()
{
  // FB must be destroyed before texture
  m_framebuffer.reset();
  g_command_buffer_mgr->DeferTextureDestruction(m_texture);
  g_state_tracker->UnbindTexture(this);
}

std::unique_ptr<MetalTexture> MetalTexture::Create(const TextureConfig& cfg)
{
  auto desc = GetTextureDescriptor(cfg);
  mtlpp::Texture device_tex = g_metal_context->GetDevice().NewTexture(desc);
  if (!device_tex)
  {
    PanicAlert("Failed to create device texture");
    return nullptr;
  }

  // TODO: FB
  std::unique_ptr<MetalFramebuffer> fb;
  if (cfg.rendertarget)
  {
    fb = MetalFramebuffer::CreateForTexture(device_tex);
    if (!fb)
    {
      PanicAlert("Failed to create framebuffer for texture");
      return nullptr;
    }
  }

  // TODO: Clear render targets
  return std::make_unique<MetalTexture>(cfg, device_tex, std::move(fb));
}

void MetalTexture::CopyRectangleFromTexture(const AbstractTexture* src,
                                            const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                            u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                            u32 dst_layer, u32 dst_level)
{
  const MetalTexture* srcentry = static_cast<const MetalTexture*>(src);
  _dbg_assert_(VIDEO, src_rect.GetWidth() == dst_rect.GetWidth() &&
                          src_rect.GetHeight() == dst_rect.GetHeight());
  _dbg_assert_(VIDEO,
               src_rect.left >= 0 && src_rect.top >= 0 && dst_rect.left >= 0 && dst_rect.top >= 0);

  // We use a command encoder on the draw buffer, this way if we're copying something
  // we drew to this frame the copy receives the updated copy.
  g_state_tracker->EndRenderPass();

  mtlpp::BlitCommandEncoder encoder = g_command_buffer_mgr->GetCurrentBuffer().BlitCommandEncoder();
  mtlpp::Origin source_origin(src_rect.left, src_rect.top, 0);
  mtlpp::Origin dest_origin(dst_rect.left, dst_rect.top, 0);
  mtlpp::Size copy_size(src_rect.GetWidth(), src_rect.GetHeight(), 1);
  encoder.Copy(srcentry->m_texture, src_layer, src_level, source_origin, copy_size, m_texture,
               dst_layer, dst_level, dest_origin);
  encoder.EndEncoding();
  ;
}

void MetalTexture::ScaleRectangleFromTexture(const AbstractTexture* source,
                                             const MathUtil::Rectangle<int>& srcrect,
                                             const MathUtil::Rectangle<int>& dstrect)
{
  _dbg_assert_(VIDEO, source->GetConfig().rendertarget);
  WARN_LOG(VIDEO, "Implement ScaleRectangleFromTexture!");
}

void MetalTexture::ResolveFromTexture(const AbstractTexture* src,
                                      const MathUtil::Rectangle<int>& rect, u32 layer, u32 level)
{
  WARN_LOG(VIDEO, "Implement ScaleRectangleFromTexture!");
}

void MetalTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                        size_t buffer_size)
{
  StreamBuffer* stream_buffer = g_metal_context->GetTextureStreamBuffer();
  _dbg_assert_(VIDEO, level < m_config.levels && width > 0 && height > 0);

  u32 block_size = Util::GetBlockSize(Util::GetMetalPixelFormat(m_config.format));
  u32 num_rows = Common::AlignUp(height, block_size) / block_size;
  u32 source_pitch = static_cast<u32>(CalculateStrideForFormat(m_config.format, row_length));
  u32 upload_size = source_pitch * num_rows;
  _dbg_assert_(VIDEO, buffer_size >= upload_size);

  // Some custom textures well exceed our streaming buffer size.
  // For these, we use a temporary buffer which we free when the cmdbuffer is completed.
  mtlpp::Buffer temp_buffer;
  u32 upload_buffer_offset = 0;
  if (upload_size >= stream_buffer->GetCurrentSize())
  {
    INFO_LOG(VIDEO, "Allocating large texture upload buffer of %u bytes",
             static_cast<unsigned>(upload_size));

    constexpr mtlpp::ResourceOptions temp_buffer_options = static_cast<mtlpp::ResourceOptions>(
        static_cast<u32>(mtlpp::ResourceOptions::HazardTrackingModeUntracked) |
        static_cast<u32>(mtlpp::ResourceOptions::StorageModeManaged));
    temp_buffer = g_metal_context->GetDevice().NewBuffer(buffer, upload_size, temp_buffer_options);
    if (!temp_buffer)
    {
      PanicAlert("Failed to allocate large texture upload buffer.");
      return;
    }
  }
  else
  {
    if (!stream_buffer->ReserveMemory(upload_size, TEXTURE_STREAMING_BUFFER_ALIGNMENT))
    {
      Renderer::GetInstance()->SubmitCommandBuffer();
      if (!stream_buffer->ReserveMemory(upload_size, TEXTURE_STREAMING_BUFFER_ALIGNMENT))
      {
        PanicAlert("Failed to reserve memory in texture streaming buffer.");
        return;
      }
    }

    upload_buffer_offset = stream_buffer->GetCurrentOffset();
    std::memcpy(stream_buffer->GetCurrentHostPointer(), buffer, upload_size);
    stream_buffer->CommitMemory(upload_size);
  }

  // Copy from the buffer to the texture.
  mtlpp::Size source_size(width, height, 1);
  mtlpp::Origin dest_origin(0, 0, 0);
  mtlpp::BlitCommandEncoder& blit_encoder = g_command_buffer_mgr->GetCurrentInitBlitEncoder();
  blit_encoder.Copy(temp_buffer ? temp_buffer : stream_buffer->GetBuffer(), upload_buffer_offset,
                    source_pitch, upload_size, source_size, m_texture, 0, level, dest_origin);

  // Defer destruction of temp buffer.
  if (temp_buffer)
    g_command_buffer_mgr->DeferBufferDestruction(temp_buffer);
}

MetalStagingTexture::MetalStagingTexture(mtlpp::Buffer& buffer, u32 stride, StagingTextureType type,
                                         const TextureConfig& config)
    : AbstractStagingTexture(type, config), m_buffer(buffer)
{
  m_map_pointer = reinterpret_cast<char*>(m_buffer.GetContents());
  m_map_stride = stride;
}

MetalStagingTexture::~MetalStagingTexture()
{
  g_command_buffer_mgr->DeferBufferDestruction(m_buffer);
}

void MetalStagingTexture::CopyFromTexture(const AbstractTexture* src,
                                          const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                          u32 src_level, const MathUtil::Rectangle<int>& dst_rect)
{
  const MetalTexture* src_tex = static_cast<const MetalTexture*>(src);
  if (m_needs_flush)
  {
    // Drop copy before reusing it.
    g_command_buffer_mgr->RemoveFencePointCallback(this);
    m_pending_command_buffer_id = 0;
    m_needs_flush = false;
  }

  // We use a command encoder on the draw buffer, this way if we're copying something
  // we drew to this frame the copy receives the updated copy.
  g_state_tracker->EndRenderPass();

  // Calculate the offset into the buffer/staging texture we're writing to.
  u32 buffer_stride = static_cast<u32>(m_config.GetStride());
  u32 buffer_offset = static_cast<u32>(dst_rect.top) * buffer_stride +
                      static_cast<u32>(dst_rect.left) * static_cast<u32>(m_texel_size);
  u32 buffer_copy_size = buffer_stride * static_cast<u32>(dst_rect.GetHeight());

  // Copy from the texture to the buffer.
  mtlpp::BlitCommandEncoder encoder = g_command_buffer_mgr->GetCurrentBuffer().BlitCommandEncoder();
  mtlpp::Origin source_origin(src_rect.left, src_rect.top, 0);
  mtlpp::Size copy_size(src_rect.GetWidth(), src_rect.GetHeight(), 1);
  encoder.Copy(src_tex->GetTexture(), src_layer, src_level, source_origin, copy_size, m_buffer,
               buffer_offset, buffer_stride, buffer_copy_size);
  encoder.EndEncoding();

  // Results are not available until the command buffer is submitted.
  m_needs_flush = true;
  g_command_buffer_mgr->AddFencePointCallback(this,
                                              [this](CommandBufferId id) {
                                                _assert_(m_needs_flush);
                                                m_pending_command_buffer_id = id;
                                              },
                                              [this](CommandBufferId id) {
                                                m_pending_command_buffer_id = 0;
                                                m_needs_flush = false;
                                                g_command_buffer_mgr->RemoveFencePointCallback(
                                                    this);
                                              });
}

void MetalStagingTexture::CopyToTexture(const MathUtil::Rectangle<int>& src_rect,
                                        AbstractTexture* dst,
                                        const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                                        u32 dst_level)
{
  _assert_(m_type == StagingTextureType::Upload);
  _assert_(src_rect.GetWidth() == dst_rect.GetWidth() &&
           src_rect.GetHeight() == dst_rect.GetHeight());
  _assert_(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= m_config.width &&
           src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= m_config.height);
  _assert_(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= dst->GetConfig().width &&
           dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= dst->GetConfig().height);

  const MetalTexture* dst_tex = static_cast<const MetalTexture*>(dst);
  if (m_needs_flush)
  {
    // Drop copy before reusing it.
    g_command_buffer_mgr->RemoveFencePointCallback(this);
    m_pending_command_buffer_id = 0;
    m_needs_flush = false;
  }

  // We use a command encoder on the draw buffer, this way if we're copying something
  // we drew to this frame the copy receives the updated copy.
  g_state_tracker->EndRenderPass();

  // Calculate the offset into the buffer/staging texture we're reading from to.
  u32 buffer_stride = static_cast<u32>(m_config.GetStride());
  u32 buffer_offset = static_cast<u32>(src_rect.top) * buffer_stride +
                      static_cast<u32>(src_rect.left) * static_cast<u32>(m_texel_size);
  u32 buffer_copy_size = buffer_stride * static_cast<u32>(src_rect.GetHeight());

  // Write to the GPU copy.
  m_buffer.DidModify(ns::Range(buffer_offset, buffer_copy_size));

  // Copy from the texture to the buffer.
  mtlpp::BlitCommandEncoder encoder = g_command_buffer_mgr->GetCurrentBuffer().BlitCommandEncoder();
  mtlpp::Origin dest_origin(dst_rect.left, dst_rect.top, 0);
  mtlpp::Size copy_size(src_rect.GetWidth(), src_rect.GetHeight(), 1);
  encoder.Copy(m_buffer, buffer_offset, buffer_stride, buffer_copy_size, copy_size,
               dst_tex->GetTexture(), dst_layer, dst_level, dest_origin);
  encoder.EndEncoding();

  // Buffer cannot be written over until the command buffer is executed.
  m_needs_flush = true;
  g_command_buffer_mgr->AddFencePointCallback(this,
                                              [this](CommandBufferId id) {
                                                _assert_(m_needs_flush);
                                                m_pending_command_buffer_id = id;
                                              },
                                              [this](CommandBufferId id) {
                                                m_pending_command_buffer_id = 0;
                                                m_needs_flush = false;
                                                g_command_buffer_mgr->RemoveFencePointCallback(
                                                    this);
                                              });
}

bool MetalStagingTexture::Map()
{
  // Shared buffers are always mapped in Metal.
  return true;
}

void MetalStagingTexture::Unmap()
{
}

void MetalStagingTexture::Flush()
{
  if (!m_needs_flush)
    return;

  // Either of the below two calls will cause the callback to fire.
  g_command_buffer_mgr->RemoveFencePointCallback(this);
  if (m_pending_command_buffer_id != 0)
  {
    // WaitForFence should fire the callback.
    g_command_buffer_mgr->WaitForCommandBuffer(m_pending_command_buffer_id);
    m_pending_command_buffer_id = 0;
  }
  else
  {
    // We don't have a fence, and are pending. That means the readback is in the current
    // command buffer, and must execute it to populate the staging texture.
    Renderer::GetInstance()->SubmitCommandBuffer(true);
  }
  m_needs_flush = false;

  // TODO: For readback textures, invalidate the CPU cache as there is new data there.
  // if (m_type == StagingTextureType::Readback)
  // m_staging_buffer->InvalidateCPUCache();
}

std::unique_ptr<MetalStagingTexture> MetalStagingTexture::Create(StagingTextureType type,
                                                                 const TextureConfig& config)
{
  // We use managed buffers for uploads, and shared buffers for downloads.
  mtlpp::ResourceOptions options = mtlpp::ResourceOptions::HazardTrackingModeUntracked;
  if (type == StagingTextureType::Upload)
  {
    options = static_cast<mtlpp::ResourceOptions>(
        static_cast<u32>(options) |
        static_cast<u32>(mtlpp::ResourceOptions::OptionCpuCacheModeWriteCombined) |
        static_cast<u32>(mtlpp::ResourceOptions::StorageModeManaged));
  }
  else
  {
    // We don't write combining for readbacks or mutables.
    options = static_cast<mtlpp::ResourceOptions>(
        static_cast<u32>(options) |
        static_cast<u32>(mtlpp::ResourceOptions::OptionCpuCacheModeDefault) |
        static_cast<u32>(mtlpp::ResourceOptions::StorageModeShared));
  }

  // Create the buffer.
  u32 stride = static_cast<u32>(config.GetStride());
  u32 buffer_size = stride * config.height;
  mtlpp::Buffer buffer = g_metal_context->GetDevice().NewBuffer(buffer_size, options);
  if (!buffer)
  {
    ERROR_LOG(VIDEO, "Failed to allocate staging buffer of %u bytes", buffer_size);
    return nullptr;
  }

  return std::make_unique<MetalStagingTexture>(buffer, stride, type, config);
}

}  // namespace Metal
