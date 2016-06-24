// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/AbstractTexture.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{
// TODO: Need a proper home and lifecycle for these
extern std::unique_ptr<D3DStreamBuffer> s_efb_copy_stream_buffer;
extern u32 s_efb_copy_last_cbuf_id;

extern ID3D12Resource* s_texture_cache_entry_readback_buffer;
extern size_t s_texture_cache_entry_readback_buffer_size;

AbstractTexture::AbstractTexture(const AbstractTextureBase::TextureConfig& config)
    : AbstractTextureBase(config)
{
  if (config.rendertarget)
  {
    m_texture =
        D3DTexture2D::Create(config.width, config.height,
                             TEXTURE_BIND_FLAG_SHADER_RESOURCE | TEXTURE_BIND_FLAG_RENDER_TARGET,
                             DXGI_FORMAT_R8G8B8A8_UNORM, 1, config.layers);

    m_texture_srv_cpu_handle = m_texture->GetSRV12CPU();
    m_texture_srv_gpu_handle = m_texture->GetSRV12GPU();
    m_texture_srv_gpu_handle_cpu_shadow = m_texture->GetSRV12GPUCPUShadow();
  }
  else
  {
    ID3D12Resource* texture_resource = nullptr;

    D3D12_RESOURCE_DESC texture_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM, config.width, config.height, 1, config.levels);

    CheckHR(D3D::device12->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC(texture_resource_desc), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr, IID_PPV_ARGS(&texture_resource)));

    m_texture = new D3DTexture2D(texture_resource, TEXTURE_BIND_FLAG_SHADER_RESOURCE,
                                 DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
                                 false, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    m_texture_srv_cpu_handle = m_texture->GetSRV12CPU();
    m_texture_srv_gpu_handle = m_texture->GetSRV12GPU();
    m_texture_srv_gpu_handle_cpu_shadow = m_texture->GetSRV12GPUCPUShadow();

    // EXISTINGD3D11TODO: better debug names
    D3D::SetDebugObjectName12(m_texture->GetTex12(), "a texture of the TextureCache");

    SAFE_RELEASE(texture_resource);
  }
}

AbstractTexture::~AbstractTexture()
{
  m_texture->Release();
}

void AbstractTexture::FromRenderTarget(u8* dst, PEControl::PixelFormat src_format,
                                       const EFBRectangle& srcRect, bool scale_by_half,
                                       unsigned int cbuf_id, const float* colmat)
{
  // When copying at half size, in multisampled mode, resolve the color/depth buffer first.
  // This is because multisampled texture reads go through Load, not Sample, and the linear
  // filter is ignored.
  bool multisampled = (g_ActiveConfig.iMultisamples > 1);
  D3DTexture2D* efb_tex = (src_format == PEControl::Z24) ?
                              FramebufferManager::GetEFBDepthTexture() :
                              FramebufferManager::GetEFBColorTexture();
  if (multisampled && scale_by_half)
  {
    multisampled = false;
    efb_tex = (src_format == PEControl::Z24) ? FramebufferManager::GetResolvedEFBDepthTexture() :
                                               FramebufferManager::GetResolvedEFBColorTexture();
  }

  // set transformation
  if (s_efb_copy_last_cbuf_id != cbuf_id)
  {
    s_efb_copy_stream_buffer->AllocateSpaceInBuffer(28 * sizeof(float), 256);

    memcpy(s_efb_copy_stream_buffer->GetCPUAddressOfCurrentAllocation(), colmat,
           28 * sizeof(float));

    s_efb_copy_last_cbuf_id = cbuf_id;
  }

  // stretch picture with increased internal resolution
  D3D::SetViewportAndScissor(0, 0, config.width, config.height);

  D3D::current_command_list->SetGraphicsRootConstantBufferView(
      DESCRIPTOR_TABLE_PS_CBVONE, s_efb_copy_stream_buffer->GetGPUAddressOfCurrentAllocation());
  D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);

  const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
  // EXISTINGD3D11TODO: try targetSource.asRECT();
  const D3D12_RECT sourcerect =
      CD3DX12_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

  // Use linear filtering if (bScaleByHalf), use point filtering otherwise
  if (scale_by_half)
    D3D::SetLinearCopySampler();
  else
    D3D::SetPointCopySampler();

  // Make sure we don't draw with the texture set as both a source and target.
  // (This can happen because we don't unbind textures when we free them.)

  m_texture->TransitionToResourceState(D3D::current_command_list,
                                       D3D12_RESOURCE_STATE_RENDER_TARGET);
  D3D::current_command_list->OMSetRenderTargets(1, &m_texture->GetRTV12(), FALSE, nullptr);

  // Create texture copy
  D3D::DrawShadedTexQuad(
      efb_tex, &sourcerect, Renderer::GetTargetWidth(), Renderer::GetTargetHeight(),
      (src_format == PEControl::Z24) ? StaticShaderCache::GetDepthMatrixPixelShader(multisampled) :
                                       StaticShaderCache::GetColorMatrixPixelShader(multisampled),
      StaticShaderCache::GetSimpleVertexShader(),
      StaticShaderCache::GetSimpleVertexShaderInputLayout(),
      StaticShaderCache::GetCopyGeometryShader(), 1.0f, 0, DXGI_FORMAT_R8G8B8A8_UNORM, false,
      m_texture->GetMultisampled());

  m_texture->TransitionToResourceState(D3D::current_command_list,
                                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

  FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(
      D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
  FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(
      D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);

  g_renderer->RestoreAPIState();
}

bool AbstractTexture::Save(const std::string& filename, unsigned int level)
{
  u32 level_width = std::max(config.width >> level, 1u);
  u32 level_height = std::max(config.height >> level, 1u);
  size_t level_pitch =
      D3D::AlignValue(level_width * sizeof(u32), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
  size_t required_readback_buffer_size = level_pitch * level_height;

  // Check if the current readback buffer is large enough
  if (required_readback_buffer_size > s_texture_cache_entry_readback_buffer_size)
  {
    // Reallocate the buffer with the new size. Safe to immediately release because we're the only
    // user and we block until completion.
    if (s_texture_cache_entry_readback_buffer)
      s_texture_cache_entry_readback_buffer->Release();

    s_texture_cache_entry_readback_buffer_size = required_readback_buffer_size;
    CheckHR(D3D::device12->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(s_texture_cache_entry_readback_buffer_size),
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&s_texture_cache_entry_readback_buffer)));
  }

  m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);

  D3D12_TEXTURE_COPY_LOCATION dst_location = {};
  dst_location.pResource = s_texture_cache_entry_readback_buffer;
  dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst_location.PlacedFootprint.Offset = 0;
  dst_location.PlacedFootprint.Footprint.Depth = 1;
  dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  dst_location.PlacedFootprint.Footprint.Width = level_width;
  dst_location.PlacedFootprint.Footprint.Height = level_height;
  dst_location.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(level_pitch);

  D3D12_TEXTURE_COPY_LOCATION src_location =
      CD3DX12_TEXTURE_COPY_LOCATION(m_texture->GetTex12(), level);

  D3D::current_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

  D3D::command_list_mgr->ExecuteQueuedWork(true);

  // Map readback buffer and save to file.
  void* readback_texture_map;
  D3D12_RANGE read_range = {0, required_readback_buffer_size};
  CheckHR(s_texture_cache_entry_readback_buffer->Map(0, &read_range, &readback_texture_map));

  bool saved = TextureToPng(static_cast<u8*>(readback_texture_map),
                            dst_location.PlacedFootprint.Footprint.RowPitch, filename,
                            dst_location.PlacedFootprint.Footprint.Width,
                            dst_location.PlacedFootprint.Footprint.Height);

  D3D12_RANGE write_range = {};
  s_texture_cache_entry_readback_buffer->Unmap(0, &write_range);
  return saved;
}

void AbstractTexture::Bind(unsigned int stage)
{
  // Textures bound as group in TextureCache::BindTextures method.
}

void AbstractTexture::CopyRectangleFromTexture(const AbstractTextureBase* source,
                                               const MathUtil::Rectangle<int>& src_rect,
                                               const MathUtil::Rectangle<int>& dst_rect)
{
  const AbstractTexture* srcentry = static_cast<const AbstractTexture*>(source);
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

    CD3DX12_BOX src_box(src_rect.left, src_rect.top, 0, src_rect.right, src_rect.bottom,
                        srcentry->config.layers);
    D3D12_TEXTURE_COPY_LOCATION dst_location =
        CD3DX12_TEXTURE_COPY_LOCATION(m_texture->GetTex12(), 0);
    D3D12_TEXTURE_COPY_LOCATION src_location =
        CD3DX12_TEXTURE_COPY_LOCATION(srcentry->m_texture->GetTex12(), 0);

    m_texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_COPY_DEST);
    srcentry->m_texture->TransitionToResourceState(D3D::current_command_list,
                                                   D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D::current_command_list->CopyTextureRegion(&dst_location, dst_rect.left, dst_rect.top, 0,
                                                 &src_location, &src_box);

    m_texture->TransitionToResourceState(D3D::current_command_list,
                                         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    srcentry->m_texture->TransitionToResourceState(D3D::current_command_list,
                                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    return;
  }
  else if (!config.rendertarget)
  {
    return;
  }

  D3D::SetViewportAndScissor(dst_rect.left, dst_rect.top, dst_rect.GetWidth(),
                             dst_rect.GetHeight());

  m_texture->TransitionToResourceState(D3D::current_command_list,
                                       D3D12_RESOURCE_STATE_RENDER_TARGET);
  D3D::current_command_list->OMSetRenderTargets(1, &m_texture->GetRTV12(), FALSE, nullptr);

  D3D::SetLinearCopySampler();

  D3D12_RECT src_rc;
  src_rc.left = src_rect.left;
  src_rc.right = src_rect.right;
  src_rc.top = src_rect.top;
  src_rc.bottom = src_rect.bottom;

  D3D::DrawShadedTexQuad(
      srcentry->m_texture, &src_rc, srcentry->config.width, srcentry->config.height,
      StaticShaderCache::GetColorCopyPixelShader(false), StaticShaderCache::GetSimpleVertexShader(),
      StaticShaderCache::GetSimpleVertexShaderInputLayout(), D3D12_SHADER_BYTECODE(), 1.0, 0,
      DXGI_FORMAT_R8G8B8A8_UNORM, false, m_texture->GetMultisampled());

  m_texture->TransitionToResourceState(D3D::current_command_list,
                                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

  FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(
      D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
  FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(
      D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);

  g_renderer->RestoreAPIState();
}

void AbstractTexture::Load(u8* data, unsigned int width, unsigned int height,
                           unsigned int expanded_width, unsigned int level)
{
  unsigned int src_pitch = 4 * expanded_width;
  D3D::ReplaceRGBATexture2D(m_texture->GetTex12(), data, width, height, src_pitch, level,
                            m_texture->GetResourceUsageState());
}
}
