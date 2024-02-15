// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include "Common/CommonTypes.h"
#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/DescriptorHeapManager.h"
#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/RenderBase.h"

namespace DX12
{
class DXTexture final : public AbstractTexture
{
public:
  ~DXTexture();

  static std::unique_ptr<DXTexture> Create(const TextureConfig& config, std::string_view name);
  static std::unique_ptr<DXTexture> CreateAdopted(ID3D12Resource* resource);

  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer, size_t buffer_size,
            u32 layer) override;
  void CopyRectangleFromTexture(const AbstractTexture* src,
                                const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                u32 dst_layer, u32 dst_level) override;
  void ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                          u32 layer, u32 level) override;
  void FinishedRendering() override;

  ID3D12Resource* GetResource() const { return m_resource.Get(); }
  const DescriptorHandle& GetSRVDescriptor() const { return m_srv_descriptor; }
  const DescriptorHandle& GetUAVDescriptor() const { return m_uav_descriptor; }
  D3D12_RESOURCE_STATES GetState() const { return m_state; }
  u32 CalcSubresource(u32 level, u32 layer) const { return level + layer * m_config.levels; }

  void TransitionToState(D3D12_RESOURCE_STATES state) const;

  // Destoys the resource backing this texture. The resource must not be in use by the GPU.
  void DestroyResource();

private:
  DXTexture(const TextureConfig& config, ID3D12Resource* resource, D3D12_RESOURCE_STATES state,
            std::string_view name);

  bool CreateSRVDescriptor();
  bool CreateUAVDescriptor();

  ComPtr<ID3D12Resource> m_resource;
  DescriptorHandle m_srv_descriptor = {};
  DescriptorHandle m_uav_descriptor = {};

  std::wstring m_name;

  mutable D3D12_RESOURCE_STATES m_state;
};

class DXFramebuffer final : public AbstractFramebuffer
{
public:
  ~DXFramebuffer() override;

  const DescriptorHandle& GetIntRTVDescriptor() const { return m_int_rtv_descriptor; }
  const DescriptorHandle& GetDSVDescriptor() const { return m_dsv_descriptor; }

  UINT GetRTVDescriptorCount() const { return static_cast<UINT>(m_render_targets.size()); }
  const D3D12_CPU_DESCRIPTOR_HANDLE* GetRTVDescriptorArray() const
  {
    return m_render_targets_raw.data();
  }
  const D3D12_CPU_DESCRIPTOR_HANDLE* GetIntRTVDescriptorArray() const;
  const D3D12_CPU_DESCRIPTOR_HANDLE* GetDSVDescriptorArray() const
  {
    return m_depth_attachment ? &m_dsv_descriptor.cpu_handle : nullptr;
  }

  void Unbind();
  void ClearRenderTargets(const ClearColor& color_value, const D3D12_RECT* rectangle) const;
  void ClearDepth(float depth_value, const D3D12_RECT* rectangle) const;
  void TransitionRenderTargets() const;

  static std::unique_ptr<DXFramebuffer>
  Create(DXTexture* color_attachment, DXTexture* depth_attachment,
         std::vector<AbstractTexture*> additional_color_attachments);

private:
  DXFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                std::vector<AbstractTexture*> additional_color_attachments,
                AbstractTextureFormat color_format, AbstractTextureFormat depth_format, u32 width,
                u32 height, u32 layers, u32 samples);

  bool CreateRTVDescriptors();
  bool CreateRTVDescriptor(u32 layers, AbstractTexture* attachment);
  bool CreateIRTVDescriptor();
  bool CreateDSVDescriptor();

  std::vector<DescriptorHandle> m_render_targets;
  std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_render_targets_raw;
  DescriptorHandle m_int_rtv_descriptor = {};
  DescriptorHandle m_dsv_descriptor = {};
};

class DXStagingTexture final : public AbstractStagingTexture
{
public:
  ~DXStagingTexture();

  void CopyFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& src_rect,
                       u32 src_layer, u32 src_level,
                       const MathUtil::Rectangle<int>& dst_rect) override;
  void CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                     const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                     u32 dst_level) override;

  bool Map() override;
  void Unmap() override;
  void Flush() override;

  static std::unique_ptr<DXStagingTexture> Create(StagingTextureType type,
                                                  const TextureConfig& config);

private:
  DXStagingTexture(StagingTextureType type, const TextureConfig& config, ID3D12Resource* resource,
                   u32 stride, u32 buffer_size);

  ComPtr<ID3D12Resource> m_resource;
  u64 m_completed_fence = 0;
  u32 m_buffer_size;
};

}  // namespace DX12
