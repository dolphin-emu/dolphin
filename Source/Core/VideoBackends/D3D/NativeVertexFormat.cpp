// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/DXShader.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/VertexManager.h"
#include "VideoCommon/NativeVertexFormat.h"

namespace DX11
{
std::mutex s_input_layout_lock;

std::unique_ptr<NativeVertexFormat>
Renderer::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<D3DVertexFormat>(vtx_decl);
}

static const DXGI_FORMAT d3d_format_lookup[5 * 4 * 2] = {
    // float formats
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8_SNORM,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16_SNORM,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_R8G8_SNORM,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R8G8B8A8_SNORM,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16G16B16A16_SNORM,
    DXGI_FORMAT_R32G32B32A32_FLOAT,

    // integer formats
    DXGI_FORMAT_R8_UINT,
    DXGI_FORMAT_R8_SINT,
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R16_SINT,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_R8G8_UINT,
    DXGI_FORMAT_R8G8_SINT,
    DXGI_FORMAT_R16G16_UINT,
    DXGI_FORMAT_R16G16_SINT,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_R8G8B8A8_UINT,
    DXGI_FORMAT_R8G8B8A8_SINT,
    DXGI_FORMAT_R16G16B16A16_UINT,
    DXGI_FORMAT_R16G16B16A16_SINT,
    DXGI_FORMAT_UNKNOWN,
};

DXGI_FORMAT VarToD3D(VarType t, int size, bool integer)
{
  DXGI_FORMAT retval = d3d_format_lookup[(int)t + 5 * (size - 1) + 5 * 4 * (int)integer];
  if (retval == DXGI_FORMAT_UNKNOWN)
  {
    PanicAlert("VarToD3D: Invalid type/size combo %i , %i, %i", (int)t, size, (int)integer);
  }
  return retval;
}

D3DVertexFormat::D3DVertexFormat(const PortableVertexDeclaration& vtx_decl)
    : NativeVertexFormat(vtx_decl)

{
  const AttributeFormat* format = &vtx_decl.position;
  if (format->enable)
  {
    m_elems[m_num_elems].SemanticName = "POSITION";
    m_elems[m_num_elems].AlignedByteOffset = format->offset;
    m_elems[m_num_elems].Format = VarToD3D(format->type, format->components, format->integer);
    m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    ++m_num_elems;
  }

  for (int i = 0; i < 3; i++)
  {
    format = &vtx_decl.normals[i];
    if (format->enable)
    {
      m_elems[m_num_elems].SemanticName = "NORMAL";
      m_elems[m_num_elems].SemanticIndex = i;
      m_elems[m_num_elems].AlignedByteOffset = format->offset;
      m_elems[m_num_elems].Format = VarToD3D(format->type, format->components, format->integer);
      m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      ++m_num_elems;
    }
  }

  for (int i = 0; i < 2; i++)
  {
    format = &vtx_decl.colors[i];
    if (format->enable)
    {
      m_elems[m_num_elems].SemanticName = "COLOR";
      m_elems[m_num_elems].SemanticIndex = i;
      m_elems[m_num_elems].AlignedByteOffset = format->offset;
      m_elems[m_num_elems].Format = VarToD3D(format->type, format->components, format->integer);
      m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      ++m_num_elems;
    }
  }

  for (int i = 0; i < 8; i++)
  {
    format = &vtx_decl.texcoords[i];
    if (format->enable)
    {
      m_elems[m_num_elems].SemanticName = "TEXCOORD";
      m_elems[m_num_elems].SemanticIndex = i;
      m_elems[m_num_elems].AlignedByteOffset = format->offset;
      m_elems[m_num_elems].Format = VarToD3D(format->type, format->components, format->integer);
      m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      ++m_num_elems;
    }
  }

  format = &vtx_decl.posmtx;
  if (format->enable)
  {
    m_elems[m_num_elems].SemanticName = "BLENDINDICES";
    m_elems[m_num_elems].AlignedByteOffset = format->offset;
    m_elems[m_num_elems].Format = VarToD3D(format->type, format->components, format->integer);
    m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    ++m_num_elems;
  }
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
  CHECK(SUCCEEDED(hr), "Failed to create input layout");

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

}  // namespace DX11
