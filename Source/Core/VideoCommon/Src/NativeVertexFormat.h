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

#ifndef _NATIVEVERTEXFORMAT_H
#define _NATIVEVERTEXFORMAT_H

#include "Common.h"

// m_components
enum
{
	VB_HAS_POSMTXIDX =(1<<1),
	VB_HAS_TEXMTXIDX0=(1<<2),
	VB_HAS_TEXMTXIDX1=(1<<3),
	VB_HAS_TEXMTXIDX2=(1<<4),
	VB_HAS_TEXMTXIDX3=(1<<5),
	VB_HAS_TEXMTXIDX4=(1<<6),
	VB_HAS_TEXMTXIDX5=(1<<7),
	VB_HAS_TEXMTXIDX6=(1<<8),
	VB_HAS_TEXMTXIDX7=(1<<9),
	VB_HAS_TEXMTXIDXALL=(0xff<<2),

	//VB_HAS_POS=0, // Implied, it always has pos! don't bother testing
	VB_HAS_NRM0=(1<<10),
	VB_HAS_NRM1=(1<<11),
	VB_HAS_NRM2=(1<<12),
	VB_HAS_NRMALL=(7<<10),

	VB_HAS_COL0=(1<<13),
	VB_HAS_COL1=(1<<14),

	VB_HAS_UV0=(1<<15),
	VB_HAS_UV1=(1<<16),
	VB_HAS_UV2=(1<<17),
	VB_HAS_UV3=(1<<18),
	VB_HAS_UV4=(1<<19),
	VB_HAS_UV5=(1<<20),
	VB_HAS_UV6=(1<<21),
	VB_HAS_UV7=(1<<22),
	VB_HAS_UVALL=(0xff<<15),
	VB_HAS_UVTEXMTXSHIFT=13,
};

#ifdef WIN32
#define LOADERDECL __cdecl
#else
#define LOADERDECL
#endif

typedef void (LOADERDECL *TPipelineFunction)();

enum VarType
{
	VAR_BYTE,
	VAR_UNSIGNED_BYTE,
	VAR_SHORT,
	VAR_UNSIGNED_SHORT,
	VAR_FLOAT,
};

struct PortableVertexDeclaration
{
	int stride;

	int num_normals;
	int normal_offset[3];
	VarType normal_gl_type;
	int normal_gl_size;
	VarType color_gl_type;  // always GL_UNSIGNED_BYTE
	int color_offset[2];
	VarType texcoord_gl_type[8];
	//int texcoord_gl_size[8];
	int texcoord_offset[8];
	int texcoord_size[8];
	int posmtx_offset;
};

// The implementation of this class is specific for GL/DX, so NativeVertexFormat.cpp
// is in the respective backend, not here in VideoCommon.

// Note that this class can't just invent arbitrary vertex formats out of its input - 
// all the data loading code must always be made compatible.
class NativeVertexFormat : NonCopyable
{
public:
	virtual ~NativeVertexFormat() {}

	virtual void Initialize(const PortableVertexDeclaration &vtx_decl) = 0;
	virtual void SetupVertexPointers() = 0;
	virtual void EnableComponents(u32 components) {}

	u32 GetVertexStride() const { return vertex_stride; }

	// TODO: move this under private:
	u32 m_components;  // VB_HAS_X. Bitmask telling what vertex components are present.

protected:
	// Let subclasses construct.
	NativeVertexFormat() {}

	u32 vertex_stride;
};

#endif  // _NATIVEVERTEXFORMAT_H
