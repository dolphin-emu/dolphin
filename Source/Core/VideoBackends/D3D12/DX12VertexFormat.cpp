// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/DX12VertexFormat.h"

#include "Common/Assert.h"
#include "Common/EnumMap.h"

#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX12
{
static DXGI_FORMAT VarToDXGIFormat(ComponentFormat t, u32 components, bool integer)
{
  using ComponentArray = std::array<DXGI_FORMAT, 4>;
  static constexpr auto f = [](ComponentArray a) { return a; };  // Deduction helper

  // NOTE: 3-component formats are not valid.
  static constexpr Common::EnumMap<ComponentArray, ComponentFormat::Float> float_type_lookup = {
      f({DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
         DXGI_FORMAT_R8G8B8A8_UNORM}),  // UByte
      f({DXGI_FORMAT_R8_SNORM, DXGI_FORMAT_R8G8_SNORM, DXGI_FORMAT_R8G8B8A8_SNORM,
         DXGI_FORMAT_R8G8B8A8_SNORM}),  // Byte
      f({DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM,
         DXGI_FORMAT_R16G16B16A16_UNORM}),  // UShort
      f({DXGI_FORMAT_R16_SNORM, DXGI_FORMAT_R16G16_SNORM, DXGI_FORMAT_R16G16B16A16_SNORM,
         DXGI_FORMAT_R16G16B16A16_SNORM}),  // Short
      f({DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT,
         DXGI_FORMAT_R32G32B32A32_FLOAT}),  // Float
  };

  static constexpr Common::EnumMap<ComponentArray, ComponentFormat::Float> integer_type_lookup = {
      f({DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_R8G8B8A8_UINT,
         DXGI_FORMAT_R8G8B8A8_UINT}),  // UByte
      f({DXGI_FORMAT_R8_SINT, DXGI_FORMAT_R8G8_SINT, DXGI_FORMAT_R8G8B8A8_SINT,
         DXGI_FORMAT_R8G8B8A8_SINT}),  // Byte
      f({DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_R16G16B16A16_UINT,
         DXGI_FORMAT_R16G16B16A16_UINT}),  // UShort
      f({DXGI_FORMAT_R16_SINT, DXGI_FORMAT_R16G16_SINT, DXGI_FORMAT_R16G16B16A16_SINT,
         DXGI_FORMAT_R16G16B16A16_SINT}),  // Short
      f({DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT,
         DXGI_FORMAT_R32G32B32A32_FLOAT}),  // Float
  };

  ASSERT(components > 0 && components <= 4);
  return integer ? integer_type_lookup[t][components - 1] : float_type_lookup[t][components - 1];
}

DXVertexFormat::DXVertexFormat(const PortableVertexDeclaration& vtx_decl)
    : NativeVertexFormat(vtx_decl)
{
  MapAttributes();
}

void DXVertexFormat::GetInputLayoutDesc(D3D12_INPUT_LAYOUT_DESC* desc) const
{
  desc->pInputElementDescs = m_attribute_descriptions.data();
  desc->NumElements = m_num_attributes;
}

void DXVertexFormat::AddAttribute(const char* semantic_name, ShaderAttrib semantic_index, u32 slot,
                                  DXGI_FORMAT format, u32 offset)
{
  ASSERT(m_num_attributes < MAX_VERTEX_ATTRIBUTES);

  auto* attr_desc = &m_attribute_descriptions[m_num_attributes];
  attr_desc->SemanticName = semantic_name;
  attr_desc->SemanticIndex = static_cast<u32>(semantic_index);
  attr_desc->Format = format;
  attr_desc->InputSlot = slot;
  attr_desc->AlignedByteOffset = offset;
  attr_desc->InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  attr_desc->InstanceDataStepRate = 0;

  m_num_attributes++;
}

void DXVertexFormat::MapAttributes()
{
  m_num_attributes = 0;

  if (m_decl.position.enable)
  {
    AddAttribute(
        "TEXCOORD", ShaderAttrib::Position, 0,
        VarToDXGIFormat(m_decl.position.type, m_decl.position.components, m_decl.position.integer),
        m_decl.position.offset);
  }

  for (u32 i = 0; i < 3; i++)
  {
    if (m_decl.normals[i].enable)
    {
      AddAttribute("TEXCOORD", ShaderAttrib::Normal + i, 0,
                   VarToDXGIFormat(m_decl.normals[i].type, m_decl.normals[i].components,
                                   m_decl.normals[i].integer),
                   m_decl.normals[i].offset);
    }
  }

  for (u32 i = 0; i < 2; i++)
  {
    if (m_decl.colors[i].enable)
    {
      AddAttribute("TEXCOORD", ShaderAttrib::Color0 + i, 0,
                   VarToDXGIFormat(m_decl.colors[i].type, m_decl.colors[i].components,
                                   m_decl.colors[i].integer),
                   m_decl.colors[i].offset);
    }
  }

  for (u32 i = 0; i < 8; i++)
  {
    if (m_decl.texcoords[i].enable)
    {
      AddAttribute("TEXCOORD", ShaderAttrib::TexCoord0 + i, 0,
                   VarToDXGIFormat(m_decl.texcoords[i].type, m_decl.texcoords[i].components,
                                   m_decl.texcoords[i].integer),
                   m_decl.texcoords[i].offset);
    }
  }

  if (m_decl.posmtx.enable)
  {
    AddAttribute(
        "TEXCOORD", ShaderAttrib::PositionMatrix, 0,
        VarToDXGIFormat(m_decl.posmtx.type, m_decl.posmtx.components, m_decl.posmtx.integer),
        m_decl.posmtx.offset);
  }
}

}  // namespace DX12
