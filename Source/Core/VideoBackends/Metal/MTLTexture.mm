// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Metal/MTLTexture.h"

#include "Common/Align.h"
#include "Common/Assert.h"

#include "VideoBackends/Metal/MTLGfx.h"
#include "VideoBackends/Metal/MTLStateTracker.h"

Metal::Texture::Texture(MRCOwned<id<MTLTexture>> tex, const TextureConfig& config)
    : AbstractTexture(config), m_tex(std::move(tex))
{
}

Metal::Texture::~Texture()
{
  if (g_state_tracker)
    g_state_tracker->UnbindTexture(m_tex);
}

void Metal::Texture::CopyRectangleFromTexture(const AbstractTexture* src,
                                              const MathUtil::Rectangle<int>& src_rect,
                                              u32 src_layer, u32 src_level,
                                              const MathUtil::Rectangle<int>& dst_rect,
                                              u32 dst_layer, u32 dst_level)
{
  g_state_tracker->EndRenderPass();
  id<MTLTexture> msrc = static_cast<const Texture*>(src)->GetMTLTexture();
  id<MTLBlitCommandEncoder> blit = [g_state_tracker->GetRenderCmdBuf() blitCommandEncoder];
  MTLSize size = MTLSizeMake(src_rect.right - src_rect.left, src_rect.bottom - src_rect.top, 1);
  [blit setLabel:@"Texture Copy"];
  [blit copyFromTexture:msrc
            sourceSlice:src_layer
            sourceLevel:src_level
           sourceOrigin:MTLOriginMake(src_rect.left, src_rect.top, 0)
             sourceSize:size
              toTexture:m_tex
       destinationSlice:dst_layer
       destinationLevel:dst_level
      destinationOrigin:MTLOriginMake(dst_rect.left, dst_rect.top, 0)];
  [blit endEncoding];
}

void Metal::Texture::ResolveFromTexture(const AbstractTexture* src,
                                        const MathUtil::Rectangle<int>& rect, u32 layer, u32 level)
{
  ASSERT(rect == MathUtil::Rectangle<int>(0, 0, src->GetWidth(), src->GetHeight()));
  id<MTLTexture> src_tex = static_cast<const Texture*>(src)->GetMTLTexture();
  g_state_tracker->ResolveTexture(src_tex, m_tex, layer, level);
}

// Use a temporary texture for large texture loads
// (Since the main upload buffer doesn't shrink after it grows)
static constexpr u32 STAGING_TEXTURE_UPLOAD_THRESHOLD = 1024 * 1024 * 4;

void Metal::Texture::Load(u32 level, u32 width, u32 height, u32 row_length,  //
                          const u8* buffer, size_t buffer_size, u32 layer)
{
  @autoreleasepool
  {
    const u32 block_size = GetBlockSizeForFormat(GetFormat());
    const u32 num_rows = Common::AlignUp(height, block_size) / block_size;
    const u32 source_pitch = CalculateStrideForFormat(m_config.format, row_length);
    const u32 upload_size = source_pitch * num_rows;
    MRCOwned<id<MTLBuffer>> tmp_buffer;
    StateTracker::Map map;
    if (upload_size > STAGING_TEXTURE_UPLOAD_THRESHOLD)
    {
      tmp_buffer = MRCTransfer([g_device
          newBufferWithLength:upload_size
                      options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined]);
      [tmp_buffer setLabel:@"Temp Texture Upload"];
      map.gpu_buffer = tmp_buffer;
      map.gpu_offset = 0;
      map.cpu_buffer = [tmp_buffer contents];
    }
    else
    {
      map = g_state_tracker->AllocateForTextureUpload(upload_size);
    }

    memcpy(map.cpu_buffer, buffer, upload_size);
    id<MTLBlitCommandEncoder> encoder = g_state_tracker->GetTextureUploadEncoder();
    [encoder copyFromBuffer:map.gpu_buffer
               sourceOffset:map.gpu_offset
          sourceBytesPerRow:source_pitch
        sourceBytesPerImage:upload_size
                 sourceSize:MTLSizeMake(width, height, 1)
                  toTexture:m_tex
           destinationSlice:layer
           destinationLevel:level
          destinationOrigin:MTLOriginMake(0, 0, 0)];
  }
}

Metal::StagingTexture::StagingTexture(MRCOwned<id<MTLBuffer>> buffer, StagingTextureType type,
                                      const TextureConfig& config)
    : AbstractStagingTexture(type, config), m_buffer(std::move(buffer))
{
  m_map_pointer = static_cast<char*>([m_buffer contents]);
  m_map_stride = config.GetStride();
}

Metal::StagingTexture::~StagingTexture() = default;

void Metal::StagingTexture::CopyFromTexture(const AbstractTexture* src,
                                            const MathUtil::Rectangle<int>& src_rect,  //
                                            u32 src_layer, u32 src_level,
                                            const MathUtil::Rectangle<int>& dst_rect)
{
  @autoreleasepool
  {
    const size_t stride = m_config.GetStride();
    const u32 offset = dst_rect.top * stride + dst_rect.left * m_texel_size;
    const MTLSize size =
        MTLSizeMake(src_rect.right - src_rect.left, src_rect.bottom - src_rect.top, 1);
    g_state_tracker->EndRenderPass();
    m_wait_buffer = MRCRetain(g_state_tracker->GetRenderCmdBuf());
    id<MTLBlitCommandEncoder> download_encoder = [m_wait_buffer blitCommandEncoder];
    [download_encoder setLabel:@"Texture Download"];
    [download_encoder copyFromTexture:static_cast<const Texture*>(src)->GetMTLTexture()
                          sourceSlice:src_layer
                          sourceLevel:src_level
                         sourceOrigin:MTLOriginMake(src_rect.left, src_rect.top, 0)
                           sourceSize:size
                             toBuffer:m_buffer
                    destinationOffset:offset
               destinationBytesPerRow:stride
             destinationBytesPerImage:stride * size.height];
    [download_encoder endEncoding];
    m_needs_flush = true;
  }
}

void Metal::StagingTexture::CopyToTexture(const MathUtil::Rectangle<int>& src_rect,  //
                                          AbstractTexture* dst,
                                          const MathUtil::Rectangle<int>& dst_rect,  //
                                          u32 dst_layer, u32 dst_level)
{
  @autoreleasepool
  {
    const size_t stride = m_config.GetStride();
    const u32 offset = dst_rect.top * stride + dst_rect.left * m_texel_size;
    const MTLSize size =
        MTLSizeMake(src_rect.right - src_rect.left, src_rect.bottom - src_rect.top, 1);
    g_state_tracker->EndRenderPass();
    m_wait_buffer = MRCRetain(g_state_tracker->GetRenderCmdBuf());
    id<MTLBlitCommandEncoder> upload_encoder = [m_wait_buffer blitCommandEncoder];
    [upload_encoder setLabel:@"Texture Upload"];
    [upload_encoder copyFromBuffer:m_buffer
                      sourceOffset:offset
                 sourceBytesPerRow:stride
               sourceBytesPerImage:stride * size.height
                        sourceSize:size
                         toTexture:static_cast<Texture*>(dst)->GetMTLTexture()
                  destinationSlice:dst_layer
                  destinationLevel:dst_level
                 destinationOrigin:MTLOriginMake(dst_rect.left, dst_rect.top, 0)];
    [upload_encoder endEncoding];
    m_needs_flush = true;
  }
}

bool Metal::StagingTexture::Map()
{
  // Always mapped
  return true;
}

void Metal::StagingTexture::Unmap()
{
  // Always mapped
}

void Metal::StagingTexture::Flush()
{
  m_needs_flush = false;
  if (!m_wait_buffer)
    return;
  if ([m_wait_buffer status] != MTLCommandBufferStatusCompleted)
  {
    // Flush while we wait, since who knows how long we'll be sitting here
    g_state_tracker->FlushEncoders();
    g_state_tracker->NotifyOfCPUGPUSync();
    [m_wait_buffer waitUntilCompleted];
  }
  m_wait_buffer = nullptr;
}

Metal::Framebuffer::Framebuffer(AbstractTexture* color, AbstractTexture* depth,  //
                                u32 width, u32 height, u32 layers, u32 samples)
    : AbstractFramebuffer(color, depth,
                          color ? color->GetFormat() : AbstractTextureFormat::Undefined,  //
                          depth ? depth->GetFormat() : AbstractTextureFormat::Undefined,  //
                          width, height, layers, samples)
{
}

Metal::Framebuffer::~Framebuffer() = default;
