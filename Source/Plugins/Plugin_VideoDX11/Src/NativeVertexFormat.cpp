// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "D3DBase.h"
#include "D3DBlob.h"
#include "NativeVertexFormat.h"
#include "VertexManager.h"
#include "VertexShaderCache.h"

namespace DX11
{

class D3DVertexFormat : public NativeVertexFormat
{
	D3D11_INPUT_ELEMENT_DESC m_elems[32];
	UINT m_num_elems;

	DX11::D3DBlob* m_vs_bytecode;
	ID3D11InputLayout* m_layout;

public:
	D3DVertexFormat() : m_num_elems(0), m_vs_bytecode(NULL), m_layout(NULL) {}
	~D3DVertexFormat() { SAFE_RELEASE(m_vs_bytecode); SAFE_RELEASE(m_layout); }

	void Initialize(const PortableVertexDeclaration &_vtx_decl);
	void SetupVertexPointers();
};

NativeVertexFormat* VertexManager::CreateNativeVertexFormat()
{
	return new D3DVertexFormat();
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

void D3DVertexFormat::SetupVertexPointers()
{
	if (m_vs_bytecode != DX11::VertexShaderCache::GetActiveShaderBytecode())
	{
		SAFE_RELEASE(m_vs_bytecode);
		SAFE_RELEASE(m_layout);

		m_vs_bytecode = DX11::VertexShaderCache::GetActiveShaderBytecode();
		m_vs_bytecode->AddRef();

		HRESULT hr = DX11::D3D::device->CreateInputLayout(m_elems, m_num_elems, m_vs_bytecode->Data(), m_vs_bytecode->Size(), &m_layout);
		if (FAILED(hr)) PanicAlert("Failed to create input layout, %s %d\n", __FILE__, __LINE__);
		DX11::D3D::SetDebugObjectName((ID3D11DeviceChild*)m_layout, "input layout used to emulate the GX pipeline");
	}
	DX11::D3D::context->IASetInputLayout(m_layout);
}

} // namespace DX11
