// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoCommon/AbstractTextureBase.h"

namespace DX12
{
class AbstractTexture : public AbstractTextureBase
{
public:
  D3DTexture2D* m_texture = nullptr;
  D3D12_CPU_DESCRIPTOR_HANDLE m_texture_srv_cpu_handle = {};
  D3D12_GPU_DESCRIPTOR_HANDLE m_texture_srv_gpu_handle = {};
  D3D12_CPU_DESCRIPTOR_HANDLE m_texture_srv_gpu_handle_cpu_shadow = {};

  AbstractTexture(const AbstractTextureBase::TextureConfig& config);
  ~AbstractTexture();

  void CopyRectangleFromTexture(const AbstractTextureBase* source,
                                const MathUtil::Rectangle<int>& src_rect,
                                const MathUtil::Rectangle<int>& dst_rect) override;

  void Load(u8* dest, unsigned int width, unsigned int height, unsigned int expanded_width,
            unsigned int levels) override;

  void FromRenderTarget(u8* dst, PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
                        bool scale_by_half, unsigned int cbuf_id, const float* colmat) override;

  void Bind(unsigned int stage) override;
  bool Save(const std::string& filename, unsigned int level) override;
};
}
