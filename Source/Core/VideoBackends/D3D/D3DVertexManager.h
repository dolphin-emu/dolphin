// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexManagerBase.h"

enum class ShaderAttrib : u32;

namespace DX11
{
class D3DVertexFormat : public NativeVertexFormat
{
public:
  D3DVertexFormat(const PortableVertexDeclaration& vtx_decl);
  ~D3DVertexFormat();
  ID3D11InputLayout* GetInputLayout(const void* vs_bytecode, size_t vs_bytecode_size);

private:
  void AddAttribute(const AttributeFormat& format, ShaderAttrib semantic_index);

  std::array<D3D11_INPUT_ELEMENT_DESC, 32> m_elems{};
  UINT m_num_elems = 0;

  std::atomic<ID3D11InputLayout*> m_layout{nullptr};
};

class VertexManager : public VertexManagerBase
{
public:
  VertexManager();
  ~VertexManager();

  bool Initialize();

  void UploadUtilityUniforms(const void* uniforms, u32 uniforms_size) override;
  bool UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                         u32* out_offset) override;
  bool UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format, u32* out_offset,
                         const void* palette_data, u32 palette_size,
                         TexelBufferFormat palette_format, u32* out_palette_offset) override;

protected:
  void ResetBuffer(u32 vertex_stride) override;
  void CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices, u32* out_base_vertex,
                    u32* out_base_index) override;
  void UploadUniforms() override;

private:
  static constexpr u32 BUFFER_COUNT = 2;
  static constexpr u32 BUFFER_SIZE =
      (VERTEX_STREAM_BUFFER_SIZE + INDEX_STREAM_BUFFER_SIZE) / BUFFER_COUNT;

  bool MapTexelBuffer(u32 required_size, D3D11_MAPPED_SUBRESOURCE& sr);

  ComPtr<ID3D11Buffer> m_buffers[BUFFER_COUNT] = {};
  u32 m_current_buffer = 0;
  u32 m_buffer_cursor = 0;

  ComPtr<ID3D11Buffer> m_vertex_constant_buffer = nullptr;
  ComPtr<ID3D11Buffer> m_geometry_constant_buffer = nullptr;
  ComPtr<ID3D11Buffer> m_pixel_constant_buffer = nullptr;

  ComPtr<ID3D11Buffer> m_texel_buffer = nullptr;
  std::array<ComPtr<ID3D11ShaderResourceView>, NUM_TEXEL_BUFFER_FORMATS> m_texel_buffer_views;
  u32 m_texel_buffer_offset = 0;
};

}  // namespace DX11
