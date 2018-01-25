// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DBlob.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/VertexManager.h"
#include "VideoBackends/D3D/VertexShaderCache.h"
#include "VideoCommon/NativeVertexFormat.h"

namespace DX11
{
std::unique_ptr<NativeVertexFormat>
VertexManager::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<D3DVertexFormat>(vtx_decl);
}

static const DXGI_FORMAT d3d_format_lookup[5 * 4 * 2] = {
    // float formats
    DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_SNORM, DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_SNORM,
    DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_SNORM, DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16_SNORM, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16G16B16A16_SNORM, DXGI_FORMAT_R32G32B32A32_FLOAT,

    // integer formats
    DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8_SINT, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_SINT,
    DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_R8G8_SINT, DXGI_FORMAT_R16G16_UINT,
    DXGI_FORMAT_R16G16_SINT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UINT,
    DXGI_FORMAT_R8G8B8A8_SINT, DXGI_FORMAT_R16G16B16A16_UINT, DXGI_FORMAT_R16G16B16A16_SINT,
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

D3DVertexFormat::D3DVertexFormat(const PortableVertexDeclaration& _vtx_decl)
{
  this->vtx_decl = _vtx_decl;

  const AttributeFormat* format = &_vtx_decl.position;
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
    format = &_vtx_decl.normals[i];
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
    format = &_vtx_decl.colors[i];
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
    format = &_vtx_decl.texcoords[i];
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

  format = &_vtx_decl.posmtx;
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
  SAFE_RELEASE(m_layout);
}

void D3DVertexFormat::SetInputLayout(D3DBlob* vs_bytecode)
{
  if (!m_layout)
  {
    // CreateInputLayout requires a shader input, but it only looks at the
    // signature of the shader, so we don't need to recompute it if the shader
    // changes.
    HRESULT hr = DX11::D3D::device->CreateInputLayout(
        m_elems.data(), m_num_elems, vs_bytecode->Data(), vs_bytecode->Size(), &m_layout);
    if (FAILED(hr))
      PanicAlert("Failed to create input layout, %s %d\n", __FILE__, __LINE__);
    DX11::D3D::SetDebugObjectName((ID3D11DeviceChild*)m_layout,
                                  "input layout used to emulate the GX pipeline");
  }
  DX11::D3D::stateman->SetInputLayout(m_layout);
}

}  // namespace DX11
