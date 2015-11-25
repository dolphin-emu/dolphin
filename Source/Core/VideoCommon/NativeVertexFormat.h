// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <functional> // for hash

#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "Common/NonCopyable.h"

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

enum VarType
{
	VAR_UNSIGNED_BYTE,  // GX_U8  = 0
	VAR_BYTE,           // GX_S8  = 1
	VAR_UNSIGNED_SHORT, // GX_U16 = 2
	VAR_SHORT,          // GX_S16 = 3
	VAR_FLOAT,          // GX_F32 = 4
};

struct AttributeFormat
{
	VarType type;
	int components;
	int offset;
	bool enable;
	bool integer;
};

struct PortableVertexDeclaration
{
	int stride;

	AttributeFormat position;
	AttributeFormat normals[3];
	AttributeFormat colors[2];
	AttributeFormat texcoords[8];
	AttributeFormat posmtx;

	inline bool operator<(const PortableVertexDeclaration& b) const
	{
		return memcmp(this, &b, sizeof(PortableVertexDeclaration)) < 0;
	}
	inline bool operator==(const PortableVertexDeclaration& b) const
	{
		return memcmp(this, &b, sizeof(PortableVertexDeclaration)) == 0;
	}
};

namespace std
{

template <>
struct hash<PortableVertexDeclaration>
{
	size_t operator()(const PortableVertexDeclaration& decl) const
	{
		return HashFletcher((u8 *) &decl, sizeof(decl));
	}
};

}

// The implementation of this class is specific for GL/DX, so NativeVertexFormat.cpp
// is in the respective backend, not here in VideoCommon.

// Note that this class can't just invent arbitrary vertex formats out of its input -
// all the data loading code must always be made compatible.
class NativeVertexFormat : NonCopyable
{
public:
	virtual ~NativeVertexFormat() {}

	virtual void SetupVertexPointers() = 0;

	u32 GetVertexStride() const { return vtx_decl.stride; }
	const PortableVertexDeclaration& GetVertexDeclaration() const { return vtx_decl; }

protected:
	// Let subclasses construct.
	NativeVertexFormat() {}

	PortableVertexDeclaration vtx_decl;
};
