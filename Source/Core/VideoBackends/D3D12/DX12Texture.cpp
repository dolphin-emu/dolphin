// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/DX12Texture.h"

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/StringUtil.h"

#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/D3D12Gfx.h"
#include "VideoBackends/D3D12/D3D12StreamBuffer.h"
#include "VideoBackends/D3D12/DX12Context.h"
#include "VideoBackends/D3D12/DescriptorHeapManager.h"

namespace DX12
{
static D3D12_BOX RectangleToBox(const MathUtil::Rectangle<int>& rc)
{
  return D3D12_BOX{static_cast<UINT>(rc.left),  static_cast<UINT>(rc.top),    0,
                   static_cast<UINT>(rc.right), static_cast<UINT>(rc.bottom), 1};
}

static ComPtr<ID3D12Resource> CreateTextureUploadBuffer(u32 buffer_size)
{
  constexpr D3D12_HEAP_PROPERTIES heap_properties = {D3D12_HEAP_TYPE_UPLOAD};
  const D3D12_RESOURCE_DESC desc = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                    0,
                                    buffer_size,
                                    1,
                                    1,
                                    1,
                                    DXGI_FORMAT_UNKNOWN,
                                    {1, 0},
                                    D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                    D3D12_RESOURCE_FLAG_NONE};

  ComPtr<ID3D12Resource> resource;
  HRESULT hr = g_dx_context->GetDevice()->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&resource));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create texture upload buffer: {}", DX12HRWrap(hr));
  return resource;
}

DXTexture::DXTexture(const TextureConfig& config, ID3D12Resource* resource,
                     D3D12_RESOURCE_STATES state, std::string_view name)
    : AbstractTexture(config), m_resource(resource), m_state(state), m_name(UTF8ToWString(name))
{
  if (!m_name.empty())
  {
    resource->SetName(m_name.c_str());
  }
}

DXTexture::~DXTexture()
{
  if (m_uav_descriptor)
  {
    g_dx_context->DeferDescriptorDestruction(g_dx_context->GetDescriptorHeapManager(),
                                             m_uav_descriptor.index);
  }

  if (m_srv_descriptor)
  {
    g_dx_context->DeferDescriptorDestruction(g_dx_context->GetDescriptorHeapManager(),
                                             m_srv_descriptor.index);
  }
  if (m_resource)
    g_dx_context->DeferResourceDestruction(m_resource.Get());
}

std::unique_ptr<DXTexture> DXTexture::Create(const TextureConfig& config, std::string_view name)
{
  constexpr D3D12_HEAP_PROPERTIES heap_properties = {D3D12_HEAP_TYPE_DEFAULT};
  D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_NONE;
  if (config.IsRenderTarget())
  {
    if (IsDepthFormat(config.format))
    {
      resource_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
      resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    else
    {
      resource_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
      resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
  }
  if (config.IsComputeImage())
    resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  const D3D12_RESOURCE_DESC resource_desc = {
      D3D12_RESOURCE_DIMENSION_TEXTURE2D,
      0,
      config.width,
      config.height,
      static_cast<u16>(config.layers),
      static_cast<u16>(config.levels),
      D3DCommon::GetDXGIFormatForAbstractFormat(config.format, config.IsRenderTarget()),
      {config.samples, 0},
      D3D12_TEXTURE_LAYOUT_UNKNOWN,
      resource_flags};

  D3D12_CLEAR_VALUE optimized_clear_value = {};
  if (config.IsRenderTarget())
  {
    optimized_clear_value.Format =
        IsDepthFormat(config.format) ?
            D3DCommon::GetDSVFormatForAbstractFormat(config.format) :
            D3DCommon::GetRTVFormatForAbstractFormat(config.format, false);
  }

  ComPtr<ID3D12Resource> resource;
  HRESULT hr = g_dx_context->GetDevice()->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, resource_state,
      config.IsRenderTarget() ? &optimized_clear_value : nullptr, IID_PPV_ARGS(&resource));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create D3D12 texture resource: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return nullptr;

  auto tex =
      std::unique_ptr<DXTexture>(new DXTexture(config, resource.Get(), resource_state, name));
  if (!tex->CreateSRVDescriptor() || (config.IsComputeImage() && !tex->CreateUAVDescriptor()))
    return nullptr;

  return tex;
}

std::unique_ptr<DXTexture> DXTexture::CreateAdopted(ID3D12Resource* resource)
{
  const D3D12_RESOURCE_DESC desc = resource->GetDesc();
  const AbstractTextureFormat format = D3DCommon::GetAbstractFormatForDXGIFormat(desc.Format);
  if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
      format == AbstractTextureFormat::Undefined)
  {
    PanicAlertFmt("Unknown format for adopted texture");
    return nullptr;
  }

  TextureConfig config(static_cast<u32>(desc.Width), desc.Height, desc.MipLevels,
                       desc.DepthOrArraySize, desc.SampleDesc.Count, format, 0,
                       AbstractTextureType::Texture_2DArray);
  if (desc.Flags &
      (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
  {
    config.flags |= AbstractTextureFlag_RenderTarget;
  }
  if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    config.flags |= AbstractTextureFlag_ComputeImage;

  auto tex =
      std::unique_ptr<DXTexture>(new DXTexture(config, resource, D3D12_RESOURCE_STATE_COMMON, ""));
  if (!tex->CreateSRVDescriptor())
    return nullptr;

  return tex;
}

bool DXTexture::CreateSRVDescriptor()
{
  if (!g_dx_context->GetDescriptorHeapManager().Allocate(&m_srv_descriptor))
  {
    PanicAlertFmt("Failed to allocate SRV descriptor");
    return false;
  }

  D3D12_SRV_DIMENSION dimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
  if (m_config.type == AbstractTextureType::Texture_2DArray)
  {
    if (m_config.IsMultisampled())
      dimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
    else
      dimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
  }
  else if (m_config.type == AbstractTextureType::Texture_2D)
  {
    if (m_config.IsMultisampled())
      dimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    else
      dimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  }
  else if (m_config.type == AbstractTextureType::Texture_CubeMap)
  {
    dimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
  }
  else
  {
    PanicAlertFmt("Failed to allocate SRV - unhandled type");
    return false;
  }
  D3D12_SHADER_RESOURCE_VIEW_DESC desc = {D3DCommon::GetSRVFormatForAbstractFormat(m_config.format),
                                          dimension, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};

  if (m_config.type == AbstractTextureType::Texture_CubeMap)
  {
    desc.TextureCube.MostDetailedMip = 0;
    desc.TextureCube.MipLevels = m_config.levels;
    desc.TextureCube.ResourceMinLODClamp = 0.0f;
  }
  else
  {
    if (m_config.IsMultisampled())
    {
      desc.Texture2DMSArray.ArraySize = m_config.layers;
    }
    else
    {
      desc.Texture2DArray.MipLevels = m_config.levels;
      desc.Texture2DArray.ArraySize = m_config.layers;
    }
  }
  g_dx_context->GetDevice()->CreateShaderResourceView(m_resource.Get(), &desc,
                                                      m_srv_descriptor.cpu_handle);
  return true;
}

bool DXTexture::CreateUAVDescriptor()
{
  if (!g_dx_context->GetDescriptorHeapManager().Allocate(&m_uav_descriptor))
  {
    PanicAlertFmt("Failed to allocate UAV descriptor");
    return false;
  }

  D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {
      D3DCommon::GetSRVFormatForAbstractFormat(m_config.format),
      D3D12_UAV_DIMENSION_TEXTURE2DARRAY};
  desc.Texture2DArray.ArraySize = m_config.layers;
  g_dx_context->GetDevice()->CreateUnorderedAccessView(m_resource.Get(), nullptr, &desc,
                                                       m_uav_descriptor.cpu_handle);

  return true;
}

void DXTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                     size_t buffer_size, u32 layer)
{
  // Textures greater than 1024*1024 will be put in staging textures that are released after
  // execution instead. A 2048x2048 texture is 16MB, and we'd only fit four of these in our
  // streaming buffer and be blocking frequently. Games are unlikely to have textures this
  // large anyway, so it's only really an issue for HD texture packs, and memory is not
  // a limiting factor in these scenarios anyway.
  constexpr u32 STAGING_BUFFER_UPLOAD_THRESHOLD = 1024 * 1024 * 4;

  // Determine the stride in the stream buffer. It must be aligned to 256 bytes.
  const u32 block_size = GetBlockSizeForFormat(GetFormat());
  const u32 num_rows = Common::AlignUp(height, block_size) / block_size;
  const u32 source_stride = CalculateStrideForFormat(m_config.format, row_length);
  const u32 upload_stride = Common::AlignUp(source_stride, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
  const u32 upload_size = upload_stride * num_rows;

  // Both paths need us in COPY_DEST state, and avoids switching back and forth for mips.
  TransitionToState(D3D12_RESOURCE_STATE_COPY_DEST);

  ComPtr<ID3D12Resource> staging_buffer;
  ID3D12Resource* upload_buffer_resource;
  void* upload_buffer_ptr;
  u32 upload_buffer_offset;
  if (upload_size >= STAGING_BUFFER_UPLOAD_THRESHOLD)
  {
    constexpr D3D12_RANGE read_range = {0, 0};
    staging_buffer = CreateTextureUploadBuffer(upload_size);
    if (!staging_buffer)
    {
      PanicAlertFmt("Failed to allocate temporary texture upload buffer");
      return;
    }
    HRESULT hr = staging_buffer->Map(0, &read_range, &upload_buffer_ptr);
    if (FAILED(hr))
    {
      PanicAlertFmt("Failed to map temporary texture upload buffer: {}", DX12HRWrap(hr));
      return;
    }

    // We defer releasing the buffer until after the command list with the copy has executed.
    g_dx_context->DeferResourceDestruction(staging_buffer.Get());
    upload_buffer_resource = staging_buffer.Get();
    upload_buffer_offset = 0;
  }
  else
  {
    if (!g_dx_context->GetTextureUploadBuffer().ReserveMemory(
            upload_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
    {
      WARN_LOG_FMT(VIDEO,
                   "Executing command list while waiting for space in texture upload buffer");
      Gfx::GetInstance()->ExecuteCommandList(false);
      if (!g_dx_context->GetTextureUploadBuffer().ReserveMemory(
              upload_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
      {
        PanicAlertFmt("Failed to allocate texture upload buffer");
        return;
      }
    }

    upload_buffer_resource = g_dx_context->GetTextureUploadBuffer().GetBuffer();
    upload_buffer_ptr = g_dx_context->GetTextureUploadBuffer().GetCurrentHostPointer();
    upload_buffer_offset = g_dx_context->GetTextureUploadBuffer().GetCurrentOffset();
  }

  // Copy in, slow path if the pitch differs.
  if (source_stride != upload_stride)
  {
    const u8* src_ptr = buffer;
    const u32 copy_size = std::min(source_stride, upload_stride);
    u8* dst_ptr = reinterpret_cast<u8*>(upload_buffer_ptr);
    for (u32 i = 0; i < num_rows; i++)
    {
      std::memcpy(dst_ptr, src_ptr, copy_size);
      src_ptr += source_stride;
      dst_ptr += upload_stride;
    }
  }
  else
  {
    std::memcpy(upload_buffer_ptr, buffer, std::min<size_t>(buffer_size, upload_size));
  }

  if (staging_buffer)
  {
    const D3D12_RANGE write_range = {0, std::min<size_t>(buffer_size, upload_size)};
    staging_buffer->Unmap(0, &write_range);
  }
  else
  {
    g_dx_context->GetTextureUploadBuffer().CommitMemory(upload_size);
  }

  // Issue copy from buffer->texture.
  const u32 aligned_width = Common::AlignUp(width, block_size);
  const u32 aligned_height = Common::AlignUp(height, block_size);
  const D3D12_TEXTURE_COPY_LOCATION dst_loc = {m_resource.Get(),
                                               D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                                               {static_cast<UINT>(CalcSubresource(level, layer))}};
  const D3D12_TEXTURE_COPY_LOCATION src_loc = {
      upload_buffer_resource,
      D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      {{upload_buffer_offset, D3DCommon::GetDXGIFormatForAbstractFormat(m_config.format, false),
        aligned_width, aligned_height, 1, upload_stride}}};
  const D3D12_BOX src_box{0, 0, 0, aligned_width, aligned_height, 1};
  g_dx_context->GetCommandList()->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, &src_box);

  // Preemptively transition to shader read only after uploading the last mip level, as we're
  // likely finished with writes to this texture for now.
  if (level == (m_config.levels - 1))
    TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void DXTexture::CopyRectangleFromTexture(const AbstractTexture* src,
                                         const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                         u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                         u32 dst_layer, u32 dst_level)
{
  const DXTexture* src_dxtex = static_cast<const DXTexture*>(src);
  ASSERT(static_cast<u32>(src_rect.right) <= src->GetWidth() &&
         static_cast<u32>(src_rect.bottom) <= src->GetHeight() && src_layer <= src->GetLayers() &&
         src_level <= src->GetLevels() && static_cast<u32>(dst_rect.right) <= GetWidth() &&
         static_cast<u32>(dst_rect.bottom) <= GetHeight() && dst_layer <= GetLayers() &&
         dst_level <= GetLevels() && src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  const D3D12_TEXTURE_COPY_LOCATION dst_loc = {
      m_resource.Get(),
      D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      {static_cast<UINT>(CalcSubresource(dst_level, dst_layer))}};
  const D3D12_TEXTURE_COPY_LOCATION src_loc = {
      src_dxtex->m_resource.Get(),
      D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      {static_cast<UINT>(src_dxtex->CalcSubresource(src_level, src_layer))}};
  const D3D12_BOX src_box = RectangleToBox(src_rect);
  const D3D12_RESOURCE_STATES old_src_state = src_dxtex->m_state;
  src_dxtex->TransitionToState(D3D12_RESOURCE_STATE_COPY_SOURCE);
  TransitionToState(D3D12_RESOURCE_STATE_COPY_DEST);

  g_dx_context->GetCommandList()->CopyTextureRegion(&dst_loc, dst_rect.left, dst_rect.top, 0,
                                                    &src_loc, &src_box);

  // Only restore the source layout. Destination is restored by FinishedRendering().
  src_dxtex->TransitionToState(old_src_state);
}

void DXTexture::ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                                   u32 layer, u32 level)
{
  const DXTexture* src_dxtex = static_cast<const DXTexture*>(src);

  D3D12_RESOURCE_STATES old_src_state = src_dxtex->m_state;
  src_dxtex->TransitionToState(D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
  TransitionToState(D3D12_RESOURCE_STATE_RESOLVE_DEST);

  g_dx_context->GetCommandList()->ResolveSubresource(
      m_resource.Get(), CalcSubresource(level, layer), src_dxtex->m_resource.Get(),
      src_dxtex->CalcSubresource(level, layer),
      D3DCommon::GetDXGIFormatForAbstractFormat(m_config.format, false));

  // Only restore the source layout. Destination is restored by FinishedRendering().
  src_dxtex->TransitionToState(old_src_state);
}

void DXTexture::FinishedRendering()
{
  if (m_state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void DXTexture::TransitionToState(D3D12_RESOURCE_STATES state) const
{
  if (m_state == state)
    return;

  ResourceBarrier(g_dx_context->GetCommandList(), m_resource.Get(), m_state, state);
  m_state = state;
}

void DXTexture::DestroyResource()
{
  if (m_uav_descriptor)
    g_dx_context->GetDescriptorHeapManager().Free(m_uav_descriptor);

  if (m_srv_descriptor)
    g_dx_context->GetDescriptorHeapManager().Free(m_srv_descriptor);

  m_resource.Reset();
}

DXFramebuffer::DXFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                             std::vector<AbstractTexture*> additional_color_attachments,
                             AbstractTextureFormat color_format, AbstractTextureFormat depth_format,
                             u32 width, u32 height, u32 layers, u32 samples)
    : AbstractFramebuffer(color_attachment, depth_attachment,
                          std::move(additional_color_attachments), color_format, depth_format,
                          width, height, layers, samples)
{
}

DXFramebuffer::~DXFramebuffer()
{
  if (m_depth_attachment)
    g_dx_context->DeferDescriptorDestruction(g_dx_context->GetDSVHeapManager(),
                                             m_dsv_descriptor.index);
  if (m_color_attachment)
  {
    if (m_int_rtv_descriptor)
    {
      g_dx_context->DeferDescriptorDestruction(g_dx_context->GetRTVHeapManager(),
                                               m_int_rtv_descriptor.index);
    }
  }
  for (auto render_target : m_render_targets)
  {
    g_dx_context->DeferDescriptorDestruction(g_dx_context->GetRTVHeapManager(),
                                             render_target.index);
  }
}

const D3D12_CPU_DESCRIPTOR_HANDLE* DXFramebuffer::GetIntRTVDescriptorArray() const
{
  if (m_color_attachment == nullptr)
    return nullptr;

  const auto& handle = m_int_rtv_descriptor.cpu_handle;

  // To save space in the descriptor heap, m_int_rtv_descriptor.cpu_handle.ptr is only allocated
  // when the integer RTV format corresponding to the current abstract format differs from the
  // non-integer RTV format. Only use the integer handle if it has been allocated.
  if (handle.ptr != 0)
    return &handle;

  // The integer and non-integer RTV formats are the same, so use the non-integer descriptor.
  return GetRTVDescriptorArray();
}

void DXFramebuffer::Unbind()
{
  static constexpr D3D12_DISCARD_REGION dr = {0, nullptr, 0, 1};
  if (HasColorBuffer())
  {
    g_dx_context->GetCommandList()->DiscardResource(
        static_cast<DXTexture*>(GetColorAttachment())->GetResource(), &dr);
  }
  for (auto additional_color_attachment : m_additional_color_attachments)
  {
    g_dx_context->GetCommandList()->DiscardResource(
        static_cast<DXTexture*>(additional_color_attachment)->GetResource(), &dr);
  }
  if (HasDepthBuffer())
  {
    g_dx_context->GetCommandList()->DiscardResource(
        static_cast<DXTexture*>(GetDepthAttachment())->GetResource(), &dr);
  }
}

void DXFramebuffer::ClearRenderTargets(const ClearColor& color_value,
                                       const D3D12_RECT* rectangle) const
{
  for (auto render_target : m_render_targets_raw)
  {
    g_dx_context->GetCommandList()->ClearRenderTargetView(render_target, color_value.data(),
                                                          rectangle ? 1 : 0, rectangle);
  }
}

void DXFramebuffer::ClearDepth(float depth_value, const D3D12_RECT* rectangle) const
{
  if (HasDepthBuffer())
  {
    g_dx_context->GetCommandList()->ClearDepthStencilView(GetDSVDescriptor().cpu_handle,
                                                          D3D12_CLEAR_FLAG_DEPTH, depth_value, 0,
                                                          rectangle ? 1 : 0, rectangle);
  }
}

void DXFramebuffer::TransitionRenderTargets() const
{
  if (HasColorBuffer())
  {
    static_cast<DXTexture*>(GetColorAttachment())
        ->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);
  }
  for (auto additional_color_attachment : m_additional_color_attachments)
  {
    static_cast<DXTexture*>(additional_color_attachment)
        ->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);
  }
}

std::unique_ptr<DXFramebuffer>
DXFramebuffer::Create(DXTexture* color_attachment, DXTexture* depth_attachment,
                      std::vector<AbstractTexture*> additional_color_attachments)
{
  if (!ValidateConfig(color_attachment, depth_attachment, additional_color_attachments))
    return nullptr;

  const AbstractTextureFormat color_format =
      color_attachment ? color_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const AbstractTextureFormat depth_format =
      depth_attachment ? depth_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const DXTexture* either_attachment = color_attachment ? color_attachment : depth_attachment;
  const u32 width = either_attachment->GetWidth();
  const u32 height = either_attachment->GetHeight();
  const u32 layers = either_attachment->GetLayers();
  const u32 samples = either_attachment->GetSamples();

  std::unique_ptr<DXFramebuffer> fb(
      new DXFramebuffer(color_attachment, depth_attachment, std::move(additional_color_attachments),
                        color_format, depth_format, width, height, layers, samples));
  if (!fb->CreateRTVDescriptors() || (color_attachment && !fb->CreateIRTVDescriptor()) ||
      (depth_attachment && !fb->CreateDSVDescriptor()))
  {
    return nullptr;
  }

  return fb;
}

bool DXFramebuffer::CreateRTVDescriptors()
{
  if (m_color_attachment)
  {
    if (!CreateRTVDescriptor(m_layers, m_color_attachment))
    {
      return false;
    }
  }

  for (auto* attachment : m_additional_color_attachments)
  {
    if (!CreateRTVDescriptor(1, attachment))
    {
      return false;
    }
  }

  return true;
}

bool DXFramebuffer::CreateRTVDescriptor(u32 layers, AbstractTexture* attachment)
{
  DescriptorHandle rtv;
  if (!g_dx_context->GetRTVHeapManager().Allocate(&rtv))
  {
    PanicAlertFmt("Failed to allocate RTV descriptor");
    return false;
  }
  m_render_targets.push_back(std::move(rtv));
  m_render_targets_raw.push_back(m_render_targets.back().cpu_handle);

  const bool multisampled = m_samples > 1;
  D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {
      D3DCommon::GetRTVFormatForAbstractFormat(m_color_format, false),
      multisampled ? D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D12_RTV_DIMENSION_TEXTURE2DARRAY};
  if (multisampled)
    rtv_desc.Texture2DMSArray.ArraySize = layers;
  else
    rtv_desc.Texture2DArray.ArraySize = layers;
  g_dx_context->GetDevice()->CreateRenderTargetView(
      static_cast<DXTexture*>(attachment)->GetResource(), &rtv_desc, m_render_targets_raw.back());

  return true;
}

bool DXFramebuffer::CreateIRTVDescriptor()
{
  const bool multisampled = m_samples > 1;
  DXGI_FORMAT non_int_format = D3DCommon::GetRTVFormatForAbstractFormat(m_color_format, false);
  DXGI_FORMAT int_format = D3DCommon::GetRTVFormatForAbstractFormat(m_color_format, true);

  // If the integer and non-integer RTV formats are the same for a given abstract format we can save
  // space in the descriptor heap by only allocating the non-integer descriptor and using it for
  // the integer RTV too.
  if (int_format != non_int_format)
  {
    if (!g_dx_context->GetRTVHeapManager().Allocate(&m_int_rtv_descriptor))
      return false;

    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {int_format, multisampled ?
                                                              D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY :
                                                              D3D12_RTV_DIMENSION_TEXTURE2DARRAY};
    if (multisampled)
      rtv_desc.Texture2DMSArray.ArraySize = m_layers;
    else
      rtv_desc.Texture2DArray.ArraySize = m_layers;
    g_dx_context->GetDevice()->CreateRenderTargetView(
        static_cast<DXTexture*>(m_color_attachment)->GetResource(), &rtv_desc,
        m_int_rtv_descriptor.cpu_handle);
  }
  return true;
}

bool DXFramebuffer::CreateDSVDescriptor()
{
  if (!g_dx_context->GetDSVHeapManager().Allocate(&m_dsv_descriptor))
  {
    PanicAlertFmt("Failed to allocate RTV descriptor");
    return false;
  }

  const bool multisampled = m_samples > 1;
  D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
      D3DCommon::GetDSVFormatForAbstractFormat(m_depth_format),
      multisampled ? D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D12_DSV_DIMENSION_TEXTURE2DARRAY,
      D3D12_DSV_FLAG_NONE};
  if (multisampled)
    dsv_desc.Texture2DMSArray.ArraySize = m_layers;
  else
    dsv_desc.Texture2DArray.ArraySize = m_layers;
  g_dx_context->GetDevice()->CreateDepthStencilView(
      static_cast<DXTexture*>(m_depth_attachment)->GetResource(), &dsv_desc,
      m_dsv_descriptor.cpu_handle);
  return true;
}

DXStagingTexture::DXStagingTexture(StagingTextureType type, const TextureConfig& config,
                                   ID3D12Resource* resource, u32 stride, u32 buffer_size)
    : AbstractStagingTexture(type, config), m_resource(resource), m_buffer_size(buffer_size)
{
  m_map_stride = stride;
}

DXStagingTexture::~DXStagingTexture()
{
  g_dx_context->DeferResourceDestruction(m_resource.Get());
}

void DXStagingTexture::CopyFromTexture(const AbstractTexture* src,
                                       const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                       u32 src_level, const MathUtil::Rectangle<int>& dst_rect)
{
  const DXTexture* src_tex = static_cast<const DXTexture*>(src);
  ASSERT(m_type == StagingTextureType::Readback || m_type == StagingTextureType::Mutable);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= src_tex->GetWidth() &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= src_tex->GetHeight());
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= m_config.width &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= m_config.height);

  const D3D12_RESOURCE_STATES old_state = src_tex->GetState();
  src_tex->TransitionToState(D3D12_RESOURCE_STATE_COPY_SOURCE);

  // Can't copy while it's mapped like in Vulkan.
  Unmap();

  // Copy from VRAM -> host-visible memory.
  const D3D12_TEXTURE_COPY_LOCATION dst_loc = {
      m_resource.Get(),
      D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      {0,
       {D3DCommon::GetDXGIFormatForAbstractFormat(m_config.format, false), m_config.width,
        m_config.height, 1u, static_cast<UINT>(m_map_stride)}}};
  const D3D12_TEXTURE_COPY_LOCATION src_loc = {
      src_tex->GetResource(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      static_cast<UINT>(src_tex->CalcSubresource(src_level, src_layer))};
  const D3D12_BOX src_box = RectangleToBox(src_rect);
  g_dx_context->GetCommandList()->CopyTextureRegion(&dst_loc, dst_rect.left, dst_rect.top, 0,
                                                    &src_loc, &src_box);

  // Restore old source texture layout.
  src_tex->TransitionToState(old_state);

  // Data is ready when the current command list is complete.
  m_needs_flush = true;
  m_completed_fence = g_dx_context->GetCurrentFenceValue();
}

void DXStagingTexture::CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                                     const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                                     u32 dst_level)
{
  const DXTexture* dst_tex = static_cast<const DXTexture*>(dst);
  ASSERT(m_type == StagingTextureType::Upload || m_type == StagingTextureType::Mutable);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= m_config.width &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= m_config.height);
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= dst_tex->GetWidth() &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= dst_tex->GetHeight());

  const D3D12_RESOURCE_STATES old_state = dst_tex->GetState();
  dst_tex->TransitionToState(D3D12_RESOURCE_STATE_COPY_DEST);

  // Can't copy while it's mapped like in Vulkan.
  Unmap();

  // Copy from VRAM -> host-visible memory.
  const D3D12_TEXTURE_COPY_LOCATION dst_loc = {
      dst_tex->GetResource(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      static_cast<UINT>(dst_tex->CalcSubresource(dst_level, dst_layer))};
  const D3D12_TEXTURE_COPY_LOCATION src_loc = {
      m_resource.Get(),
      D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      {0,
       {D3DCommon::GetDXGIFormatForAbstractFormat(m_config.format, false), m_config.width,
        m_config.height, 1u, static_cast<UINT>(m_map_stride)}}};
  const D3D12_BOX src_box = RectangleToBox(src_rect);
  g_dx_context->GetCommandList()->CopyTextureRegion(&dst_loc, dst_rect.left, dst_rect.top, 0,
                                                    &src_loc, &src_box);

  // Restore old source texture layout.
  dst_tex->TransitionToState(old_state);

  // Data is ready when the current command list is complete.
  m_needs_flush = true;
  m_completed_fence = g_dx_context->GetCurrentFenceValue();
}

bool DXStagingTexture::Map()
{
  if (m_map_pointer)
    return true;

  const D3D12_RANGE read_range = {0u, m_type == StagingTextureType::Upload ? 0u : m_buffer_size};
  HRESULT hr = m_resource->Map(0, &read_range, reinterpret_cast<void**>(&m_map_pointer));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Map resource failed: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return false;

  return true;
}

void DXStagingTexture::Unmap()
{
  if (!m_map_pointer)
    return;

  const D3D12_RANGE write_range = {0u, m_type != StagingTextureType::Upload ? 0 : m_buffer_size};
  m_resource->Unmap(0, &write_range);
  m_map_pointer = nullptr;
}

void DXStagingTexture::Flush()
{
  if (!m_needs_flush)
    return;

  m_needs_flush = false;

  // If the completed fence is the same as the current command buffer fence, we need to execute
  // the current list and wait for it to complete. This is the slowest path. Otherwise, if the
  // command list with the copy has been submitted, we only need to wait for the fence.
  if (m_completed_fence == g_dx_context->GetCurrentFenceValue())
    Gfx::GetInstance()->ExecuteCommandList(true);
  else
    g_dx_context->WaitForFence(m_completed_fence);
}

std::unique_ptr<DXStagingTexture> DXStagingTexture::Create(StagingTextureType type,
                                                           const TextureConfig& config)
{
  ASSERT(config.levels == 1 && config.layers == 1 && config.samples == 1);

  // Readback and mutable share the same heap type.
  const bool is_upload = type == StagingTextureType::Upload;
  const D3D12_HEAP_PROPERTIES heap_properties = {is_upload ? D3D12_HEAP_TYPE_UPLOAD :
                                                             D3D12_HEAP_TYPE_READBACK};

  const u32 texel_size = AbstractTexture::GetTexelSizeForFormat(config.format);
  const u32 stride = Common::AlignUp(config.width * texel_size, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
  const u32 size = stride * config.height;

  const D3D12_RESOURCE_DESC desc = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                    0,
                                    size,
                                    1,
                                    1,
                                    1,
                                    DXGI_FORMAT_UNKNOWN,
                                    {1, 0},
                                    D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                    D3D12_RESOURCE_FLAG_NONE};

  // Readback textures are stuck in COPY_DEST and are never GPU readable.
  // Upload textures are stuck in GENERIC_READ, and are never CPU readable.
  ComPtr<ID3D12Resource> resource;
  HRESULT hr = g_dx_context->GetDevice()->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &desc,
      is_upload ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(&resource));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create staging texture resource: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return nullptr;

  return std::unique_ptr<DXStagingTexture>(
      new DXStagingTexture(type, config, resource.Get(), stride, size));
}

}  // namespace DX12
