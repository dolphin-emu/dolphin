// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include "Common/Assert.h"
#include "Common/EnumMap.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DGfx.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DVertexManager.h"
#include "VideoBackends/D3D/DXShader.h"
#include "VideoCommon/NativeVertexFormat.h"

namespace DX11
{
std::mutex s_input_layout_lock;

std::unique_ptr<NativeVertexFormat>
Gfx::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<D3DVertexFormat>(vtx_decl);
}

DXGI_FORMAT VarToD3D(ComponentFormat t, int size, bool integer)
{
  using FormatMap = Common::EnumMap<DXGI_FORMAT, ComponentFormat::Float>;
  static constexpr auto f = [](FormatMap a) { return a; };  // Deduction helper

  static constexpr std::array<FormatMap, 4> d3d_float_format_lookup = {
      f({
          DXGI_FORMAT_R8_UNORM,
          DXGI_FORMAT_R8_SNORM,
          DXGI_FORMAT_R16_UNORM,
          DXGI_FORMAT_R16_SNORM,
          DXGI_FORMAT_R32_FLOAT,
      }),
      f({
          DXGI_FORMAT_R8G8_UNORM,
          DXGI_FORMAT_R8G8_SNORM,
          DXGI_FORMAT_R16G16_UNORM,
          DXGI_FORMAT_R16G16_SNORM,
          DXGI_FORMAT_R32G32_FLOAT,
      }),
      f({
          DXGI_FORMAT_UNKNOWN,
          DXGI_FORMAT_UNKNOWN,
          DXGI_FORMAT_UNKNOWN,
          DXGI_FORMAT_UNKNOWN,
          DXGI_FORMAT_R32G32B32_FLOAT,
      }),
      f({
          DXGI_FORMAT_R8G8B8A8_UNORM,
          DXGI_FORMAT_R8G8B8A8_SNORM,
          DXGI_FORMAT_R16G16B16A16_UNORM,
          DXGI_FORMAT_R16G16B16A16_SNORM,
          DXGI_FORMAT_R32G32B32A32_FLOAT,
      }),
  };

  static constexpr std::array<FormatMap, 4> d3d_integer_format_lookup = {
      f({
          DXGI_FORMAT_R8_UINT,
          DXGI_FORMAT_R8_SINT,
          DXGI_FORMAT_R16_UINT,
          DXGI_FORMAT_R16_SINT,
          DXGI_FORMAT_UNKNOWN,
      }),
      f({
          DXGI_FORMAT_R8G8_UINT,
          DXGI_FORMAT_R8G8_SINT,
          DXGI_FORMAT_R16G16_UINT,
          DXGI_FORMAT_R16G16_SINT,
          DXGI_FORMAT_UNKNOWN,
      }),
      f({
          DXGI_FORMAT_UNKNOWN,
          DXGI_FORMAT_UNKNOWN,
          DXGI_FORMAT_UNKNOWN,
          DXGI_FORMAT_UNKNOWN,
          DXGI_FORMAT_UNKNOWN,
      }),
      f({
          DXGI_FORMAT_R8G8B8A8_UINT,
          DXGI_FORMAT_R8G8B8A8_SINT,
          DXGI_FORMAT_R16G16B16A16_UINT,
          DXGI_FORMAT_R16G16B16A16_SINT,
          DXGI_FORMAT_UNKNOWN,
      }),
  };

  DXGI_FORMAT retval =
      integer ? d3d_integer_format_lookup[size - 1][t] : d3d_float_format_lookup[size - 1][t];
  if (retval == DXGI_FORMAT_UNKNOWN)
  {
    PanicAlertFmt("VarToD3D: Invalid type/size combo {}, {}, {}", t, size, integer);
  }
  return retval;
}

D3DVertexFormat::D3DVertexFormat(const PortableVertexDeclaration& vtx_decl)
    : NativeVertexFormat(vtx_decl)

{
  AddAttribute(vtx_decl.position, ShaderAttrib::Position);

  for (u32 i = 0; i < 3; i++)
    AddAttribute(vtx_decl.normals[i], ShaderAttrib::Normal + i);

  for (u32 i = 0; i < 2; i++)
    AddAttribute(vtx_decl.colors[i], ShaderAttrib::Color0 + i);

  for (u32 i = 0; i < 8; i++)
    AddAttribute(vtx_decl.texcoords[i], ShaderAttrib::TexCoord0 + i);

  AddAttribute(vtx_decl.posmtx, ShaderAttrib::PositionMatrix);
}

D3DVertexFormat::~D3DVertexFormat()
{
  ID3D11InputLayout* layout = m_layout.load();
  if (layout)
    layout->Release();
}

ID3D11InputLayout* D3DVertexFormat::GetInputLayout(const void* vs_bytecode, size_t vs_bytecode_size)
{
  // CreateInputLayout requires a shader input, but it only looks at the signature of the shader,
  // so we don't need to recompute it if the shader changes.
  ID3D11InputLayout* layout = m_layout.load();
  if (layout)
    return layout;

  HRESULT hr = D3D::device->CreateInputLayout(m_elems.data(), m_num_elems, vs_bytecode,
                                              vs_bytecode_size, &layout);
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create input layout: {}", DX11HRWrap(hr));

  // This method can be called from multiple threads, so ensure that only one thread sets the
  // cached input layout pointer. If another thread beats this thread, use the existing layout.
  ID3D11InputLayout* expected = nullptr;
  if (!m_layout.compare_exchange_strong(expected, layout))
  {
    if (layout)
      layout->Release();

    layout = expected;
  }

  return layout;
}

void D3DVertexFormat::AddAttribute(const AttributeFormat& format, ShaderAttrib semantic_index)
{
  if (format.enable)
  {
    m_elems[m_num_elems].SemanticName = "TEXCOORD";
    m_elems[m_num_elems].SemanticIndex = static_cast<u32>(semantic_index);
    m_elems[m_num_elems].AlignedByteOffset = format.offset;
    m_elems[m_num_elems].Format = VarToD3D(format.type, format.components, format.integer);
    m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    ++m_num_elems;
  }
}

}  // namespace DX11
