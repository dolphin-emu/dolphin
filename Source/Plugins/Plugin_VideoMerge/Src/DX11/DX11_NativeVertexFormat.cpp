
// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// Common
#include "MemoryUtil.h"
#include "x64Emitter.h"
#include "ABI.h"

// VideoCommon
#include "Profiler.h"
#include "CPMemory.h"
#include "VertexShaderGen.h"
#include "NativeVertexFormat.h"

// DX11
#include "DX11_D3DBase.h"
#include "DX11_VertexShaderCache.h"
#include "DX11_VertexManager.h"

namespace DX11
{

class D3DVertexFormat : public NativeVertexFormat
{
	D3D11_INPUT_ELEMENT_DESC m_elems[32];
	UINT m_num_elems;

public:
	D3DVertexFormat()  : m_num_elems(0) {}
	void Initialize(const PortableVertexDeclaration &_vtx_decl);
	void SetupVertexPointers() const;
};

NativeVertexFormat* VertexManager::CreateNativeVertexFormat()
{
	return new D3DVertexFormat;
}

DXGI_FORMAT VarToD3D(VarType t, int size)
{
	DXGI_FORMAT retval = DXGI_FORMAT_UNKNOWN;
	static const DXGI_FORMAT lookup1[5] = {
		DXGI_FORMAT_R8_SNORM, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R16_SNORM, DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R32_FLOAT
	};
	static const DXGI_FORMAT lookup2[5] = {
		DXGI_FORMAT_R8G8_SNORM, DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R16G16_SNORM, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R32G32_FLOAT
	};
	static const DXGI_FORMAT lookup3[5] = {
		DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32G32B32_FLOAT
	};
	static const DXGI_FORMAT lookup4[5] = {
		DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16B16A16_SNORM, DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R32G32B32A32_FLOAT
	};

	switch (size)
	{
		case 1: retval = lookup1[t]; break;
		case 2: retval = lookup2[t]; break;
		case 3: retval = lookup3[t]; break;
		case 4: retval = lookup4[t]; break;
		default: break;
	}
	if (retval == DXGI_FORMAT_UNKNOWN)
	{
		PanicAlert("VarToD3D: Invalid type/size combo %i , %i", (int)t, size);
	}
	return retval;
}

void D3DVertexFormat::Initialize(const PortableVertexDeclaration &_vtx_decl)
{
	vertex_stride = _vtx_decl.stride;
	memset(m_elems, 0, sizeof(m_elems));

	m_elems[m_num_elems].SemanticName = "POSITION";
	m_elems[m_num_elems].AlignedByteOffset = 0;
	m_elems[m_num_elems].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	++m_num_elems;

	for (int i = 0; i < 3; i++)
	{
		if (_vtx_decl.normal_offset[i] > 0) 
		{
			m_elems[m_num_elems].SemanticName = "NORMAL";
			m_elems[m_num_elems].SemanticIndex = i;
			m_elems[m_num_elems].AlignedByteOffset = _vtx_decl.normal_offset[i];
			m_elems[m_num_elems].Format = VarToD3D(_vtx_decl.normal_gl_type, _vtx_decl.normal_gl_size);
			m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			++m_num_elems;
		}
	}

	for (int i = 0; i < 2; i++)
	{
		if (_vtx_decl.color_offset[i] > 0) 
		{
			m_elems[m_num_elems].SemanticName = "COLOR";
			m_elems[m_num_elems].SemanticIndex = i;
			m_elems[m_num_elems].AlignedByteOffset = _vtx_decl.color_offset[i];
			m_elems[m_num_elems].Format = VarToD3D(_vtx_decl.color_gl_type, 4);
			m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			++m_num_elems;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		if (_vtx_decl.texcoord_offset[i] > 0)
		{
			m_elems[m_num_elems].SemanticName = "TEXCOORD";
			m_elems[m_num_elems].SemanticIndex = i;
			m_elems[m_num_elems].AlignedByteOffset = _vtx_decl.texcoord_offset[i];
			m_elems[m_num_elems].Format = VarToD3D(_vtx_decl.texcoord_gl_type[i], _vtx_decl.texcoord_size[i]);
			m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			++m_num_elems;
		}
	}

	if (_vtx_decl.posmtx_offset != -1)
	{
		m_elems[m_num_elems].SemanticName = "BLENDINDICES";
		m_elems[m_num_elems].AlignedByteOffset = _vtx_decl.posmtx_offset;
		m_elems[m_num_elems].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		++m_num_elems;
	}
}

void D3DVertexFormat::SetupVertexPointers() const
{
	D3D::gfxstate->SetInputElements(m_elems, m_num_elems);
}

}
