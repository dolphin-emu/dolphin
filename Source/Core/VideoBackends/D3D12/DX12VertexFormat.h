// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <d3d12.h>

#include "Common/CommonTypes.h"
#include "VideoCommon/NativeVertexFormat.h"

enum class ShaderAttrib : u32;

namespace DX12
{
class DXVertexFormat : public NativeVertexFormat
{
public:
  static constexpr u32 MAX_VERTEX_ATTRIBUTES = 16;

  DXVertexFormat(const PortableVertexDeclaration& vtx_decl);

  // Passed to pipeline state creation
  void GetInputLayoutDesc(D3D12_INPUT_LAYOUT_DESC* desc) const;

private:
  void AddAttribute(const char* semantic_name, ShaderAttrib semantic_index, u32 slot,
                    DXGI_FORMAT format, u32 offset);
  void MapAttributes();

  std::array<D3D12_INPUT_ELEMENT_DESC, MAX_VERTEX_ATTRIBUTES> m_attribute_descriptions = {};
  u32 m_num_attributes = 0;
};
}  // namespace DX12
