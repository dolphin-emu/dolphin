// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "D3DBase.h"

#include "x64Emitter.h"
#include "x64ABI.h"
#include "MemoryUtil.h"
#include "VertexShaderGen.h"

#include "CPMemory.h"
#include "NativeVertexFormat.h"
#include "VertexManager.h"

namespace DX9
{

class D3DVertexFormat : public NativeVertexFormat
{
	LPDIRECT3DVERTEXDECLARATION9 d3d_decl;

public:
	D3DVertexFormat() : d3d_decl(NULL) {}
	~D3DVertexFormat();
	virtual void Initialize(const PortableVertexDeclaration &_vtx_decl);
	virtual void SetupVertexPointers();

#if defined(_DEBUG) || defined(DEBUGFAST)
	D3DVERTEXELEMENT9 elements[32];
	int num_elements;
#endif
};

NativeVertexFormat* VertexManager::CreateNativeVertexFormat()
{
	return new D3DVertexFormat();
}

void DX9::VertexManager::GetElements(NativeVertexFormat* format, D3DVERTEXELEMENT9** elems, int* num)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	*elems = ((D3DVertexFormat*)format)->elements;
	*num = ((D3DVertexFormat*)format)->num_elements;
#else
	*elems = NULL;
	*num = 0;
#endif
}

D3DVertexFormat::~D3DVertexFormat()
{
	if (d3d_decl)
	{
		d3d_decl->Release();
		d3d_decl = NULL;
	}
}

D3DDECLTYPE VarToD3D(VarType t, int size)
{
	if (t < 0 || t > 4) {
		PanicAlert("VarToD3D: Invalid VarType %i", t);
	}
	static const D3DDECLTYPE lookup1[5] = {
		D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_FLOAT1,
	};
	static const D3DDECLTYPE lookup2[5] = {
		D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_SHORT2N, D3DDECLTYPE_USHORT2N, D3DDECLTYPE_FLOAT2,
	};
	static const D3DDECLTYPE lookup3[5] = {
		D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_FLOAT3,
	};
	// Sadly, D3D9 has no SBYTE4N. D3D10 does, though.
	static const D3DDECLTYPE lookup4[5] = {
		D3DDECLTYPE_UNUSED, D3DDECLTYPE_UBYTE4N, D3DDECLTYPE_SHORT4N, D3DDECLTYPE_USHORT4N, D3DDECLTYPE_FLOAT4,
	};
	D3DDECLTYPE retval = D3DDECLTYPE_UNUSED;
	switch (size)
	{
	case 1: retval = lookup1[t]; break;
	case 2: retval = lookup2[t]; break;
	case 3: retval = lookup3[t]; break;
	case 4: retval = lookup4[t]; break;
	default: break;
	}
	if (retval == D3DDECLTYPE_UNUSED) {
		PanicAlert("VarToD3D: Invalid type/size combo %i , %i", (int)t, size);
	}
	return retval;
}

void D3DVertexFormat::Initialize(const PortableVertexDeclaration &_vtx_decl)
{
	vertex_stride = _vtx_decl.stride;

	D3DVERTEXELEMENT9 elems[32];
	memset(elems, 0, sizeof(elems));

	// There's only one stream and it's 0, so the above memset takes care of that - no need to set Stream.
	// Same for method.
	
	// So, here we go. First position:
	int elem_idx = 0;
	elems[elem_idx].Offset = 0;  // Positions are always first, at position 0. Always float3.
	elems[elem_idx].Type = D3DDECLTYPE_FLOAT3;
	elems[elem_idx].Usage = D3DDECLUSAGE_POSITION;
	++elem_idx;

	for (int i = 0; i < 3; i++)
	{
		if (_vtx_decl.normal_offset[i] > 0) 
		{
			elems[elem_idx].Offset = _vtx_decl.normal_offset[i];
			elems[elem_idx].Type = VarToD3D(_vtx_decl.normal_gl_type, _vtx_decl.normal_gl_size);
			elems[elem_idx].Usage = D3DDECLUSAGE_NORMAL;
			elems[elem_idx].UsageIndex = i;
			++elem_idx;
		}
	}

	for (int i = 0; i < 2; i++)
	{
		if (_vtx_decl.color_offset[i] > 0) 
		{
			elems[elem_idx].Offset = _vtx_decl.color_offset[i];
			elems[elem_idx].Type = VarToD3D(_vtx_decl.color_gl_type, 4);
			elems[elem_idx].Usage = D3DDECLUSAGE_COLOR;
			elems[elem_idx].UsageIndex = i;
			++elem_idx;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		if (_vtx_decl.texcoord_offset[i] > 0)
		{
			elems[elem_idx].Offset = _vtx_decl.texcoord_offset[i];
			elems[elem_idx].Type = VarToD3D(_vtx_decl.texcoord_gl_type[i], _vtx_decl.texcoord_size[i]);
			elems[elem_idx].Usage = D3DDECLUSAGE_TEXCOORD;
			elems[elem_idx].UsageIndex = i;
			++elem_idx;
		}
	}

	if (_vtx_decl.posmtx_offset != -1)
	{
		elems[elem_idx].Offset = _vtx_decl.posmtx_offset;
		elems[elem_idx].Usage = D3DDECLUSAGE_BLENDINDICES;
		elems[elem_idx].Type = D3DDECLTYPE_D3DCOLOR;
		elems[elem_idx].UsageIndex = 0;
		++elem_idx;
	}

	// End marker
	elems[elem_idx].Stream = 0xff;
	elems[elem_idx].Type = D3DDECLTYPE_UNUSED;
	++elem_idx;

	if (FAILED(DX9::D3D::dev->CreateVertexDeclaration(elems, &d3d_decl)))
	{
		PanicAlert("Failed to create D3D vertex declaration!");
		return;
	}
#if defined(_DEBUG) || defined(DEBUGFAST)
	memcpy(&elements, elems, sizeof(elems));
	num_elements = elem_idx;
#endif
}

void D3DVertexFormat::SetupVertexPointers()
{
	if (d3d_decl)
		DX9::D3D::SetVertexDeclaration(d3d_decl);
	else
		ERROR_LOG(VIDEO, "Invalid D3D decl");
}

} // namespace DX9
