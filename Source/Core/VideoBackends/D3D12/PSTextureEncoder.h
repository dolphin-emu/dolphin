// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/VideoCommon.h"

namespace DX12
{
class PSTextureEncoder final
{
public:
  PSTextureEncoder();

  void Init();
  void Shutdown();
  void Encode(u8* dst, const EFBCopyFormat& format, u32 native_width, u32 bytes_per_row,
              u32 num_blocks_y, u32 memory_stride, bool is_depth_copy, const EFBRectangle& src_rect,
              bool scale_by_half);

private:
  D3D12_SHADER_BYTECODE GetEncodingPixelShader(const EFBCopyFormat& format);

  bool m_ready = false;

  ID3D12Resource* m_out = nullptr;
  D3D12_CPU_DESCRIPTOR_HANDLE m_out_rtv_cpu = {};

  ID3D12Resource* m_out_readback_buffer = nullptr;

  ID3D12Resource* m_encode_params_buffer = nullptr;
  void* m_encode_params_buffer_data = nullptr;

  std::map<EFBCopyFormat, D3D12_SHADER_BYTECODE> m_encoding_shaders;
  std::vector<ID3DBlob*> m_shader_blobs;
};
}
