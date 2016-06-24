// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

struct TCacheEntry : TCacheEntryBase
{
  D3DTexture2D* const m_texture = nullptr;
  D3D12_CPU_DESCRIPTOR_HANDLE m_texture_srv_cpu_handle = {};
  D3D12_GPU_DESCRIPTOR_HANDLE m_texture_srv_gpu_handle = {};
  D3D12_CPU_DESCRIPTOR_HANDLE m_texture_srv_gpu_handle_cpu_shadow = {};

  TCacheEntry(const TCacheEntryConfig& config, D3DTexture2D* tex)
      : TCacheEntryBase(config), m_texture(tex)
  {
  }
  ~TCacheEntry();

  void CopyRectangleFromTexture(const TCacheEntryBase* source,
                                const MathUtil::Rectangle<int>& src_rect,
                                const MathUtil::Rectangle<int>& dst_rect) override;

  void Load(unsigned int width, unsigned int height, unsigned int expanded_width,
            unsigned int levels) override;

  void FromRenderTarget(u8* dst, PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
                        bool scale_by_half, unsigned int cbuf_id, const float* colmat) override;

  void Bind(unsigned int stage) override;
  bool Save(const std::string& filename, unsigned int level) override;
};