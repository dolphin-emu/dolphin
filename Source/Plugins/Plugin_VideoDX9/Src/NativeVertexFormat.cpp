
// Copyright (C) 2003-2008 Dolphin Project.

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

#include "D3DBase.h"

#include "Profiler.h"
#include "x64Emitter.h"
#include "ABI.h"
#include "MemoryUtil.h"
#include "VertexShaderGen.h"

#include "CPMemory.h"
#include "NativeVertexFormat.h"


class D3DVertexFormat : public NativeVertexFormat
{
	LPDIRECT3DVERTEXDECLARATION9 d3d_decl;

public:
	D3DVertexFormat();
	~D3DVertexFormat();
	virtual void Initialize(const PortableVertexDeclaration &_vtx_decl);
	virtual void SetupVertexPointers() const;
};

NativeVertexFormat *NativeVertexFormat::Create()
{
	return new D3DVertexFormat();
}

D3DVertexFormat::D3DVertexFormat() : d3d_decl(NULL)
{
}

D3DVertexFormat::~D3DVertexFormat()
{
	if (d3d_decl)
	{
		d3d_decl->Release();
		d3d_decl = NULL;
	}
}

D3DDECLTYPE VarToD3D(VarType t)
{
	static const D3DDECLTYPE lookup[5] =
	{
		D3DDECLTYPE_UBYTE4, D3DDECLTYPE_UBYTE4, D3DDECLTYPE_SHORT4N, D3DDECLTYPE_USHORT4N, D3DDECLTYPE_FLOAT3,
	};
	return lookup[t];
}

// TODO: Ban signed bytes as normals - not likely that ATI supports them natively.
// We probably won't see much of a speed loss, and any speed loss will be regained anyway
// when we finally compile display lists.
void D3DVertexFormat::Initialize(const PortableVertexDeclaration &_vtx_decl)
{
	vertex_stride = _vtx_decl.stride;

	D3DVERTEXELEMENT9 *elems = new D3DVERTEXELEMENT9[32];
	memset(elems, 0, sizeof(D3DVERTEXELEMENT9) * 32);

	// There's only one stream and it's 0, so the above memset takes care of that - no need to set Stream.
	// Same for method.
	
	// So, here we go. First position:
	int elem_idx = 0;
	elems[elem_idx].Offset = 0;  // Positions are always first, at position 0.
	elems[elem_idx].Type = D3DDECLTYPE_FLOAT3;
	elems[elem_idx].Usage = D3DDECLUSAGE_POSITION;
	++elem_idx;

	for (int i = 0; i < 3; i++)
	{
		if (_vtx_decl.normal_offset[i] >= 0) 
		{
			elems[elem_idx].Offset = _vtx_decl.normal_offset[i];
			elems[elem_idx].Type = VarToD3D(_vtx_decl.normal_gl_type);
			elems[elem_idx].Usage = D3DDECLUSAGE_NORMAL;
			elems[elem_idx].UsageIndex = i;
			++elem_idx;
		}
	}

	for (int i = 0; i < 2; i++)
	{
		if (_vtx_decl.color_offset[i] >= 0) 
		{
			elems[elem_idx].Offset = _vtx_decl.color_offset[i];
			elems[elem_idx].Type = VarToD3D(_vtx_decl.color_gl_type);
			elems[elem_idx].Usage = D3DDECLUSAGE_COLOR;
			elems[elem_idx].UsageIndex = i;
			++elem_idx;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		if (_vtx_decl.texcoord_offset[i] >= 0)
		{
			elems[elem_idx].Offset = _vtx_decl.texcoord_offset[i];
			elems[elem_idx].Type = VarToD3D(_vtx_decl.texcoord_gl_type[i]);
			elems[elem_idx].Usage = D3DDECLUSAGE_TEXCOORD;
			elems[elem_idx].UsageIndex = i;
			++elem_idx;
		}
	}

	if (_vtx_decl.posmtx_offset != -1)
	{
		PanicAlert("Posmtx stream not supported correctly. %i", _vtx_decl.posmtx_offset);
		// glVertexAttribPointer(SHADER_POSMTX_ATTRIB, 4, GL_UNSIGNED_BYTE, GL_FALSE, vtx_decl.stride, (void *)vtx_decl.posmtx_offset);
		elems[elem_idx].Offset = _vtx_decl.posmtx_offset;
		elems[elem_idx].Usage = D3DDECLUSAGE_BLENDINDICES;
		elems[elem_idx].UsageIndex = 0;
		++elem_idx;
	}

	// End marker
	elems[elem_idx].Stream = 0xff;
	elems[elem_idx].Type = D3DDECLTYPE_UNUSED;
	++elem_idx;

	if (FAILED(D3D::dev->CreateVertexDeclaration(elems, &d3d_decl)))
	{
		PanicAlert("Failed to create D3D vertex declaration!");
		return;
	}
}

void D3DVertexFormat::SetupVertexPointers() const
{
	D3D::dev->SetVertexDeclaration(d3d_decl);
}