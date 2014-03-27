// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"
#include "Common/Common.h"

union VertexComponents
{
	// NOTE: Having a "position" component is always implied
	//BitField< 0,1,u32> has_position;
	BitField< 1,1,u32> has_posmtxidx;

	BitField< 2,1,u32> has_texmtxidx0;
	BitField< 3,1,u32> has_texmtxidx1;
	BitField< 4,1,u32> has_texmtxidx2;
	BitField< 5,1,u32> has_texmtxidx3;
	BitField< 6,1,u32> has_texmtxidx4;
	BitField< 7,1,u32> has_texmtxidx5;
	BitField< 8,1,u32> has_texmtxidx6;
	BitField< 9,1,u32> has_texmtxidx7;

	BitField<10,1,u32> has_normal0;
	BitField<11,1,u32> has_normal1;
	BitField<12,1,u32> has_normal2;

	BitField<13,1,u32> has_color0;
	BitField<14,1,u32> has_color1;

	BitField<15,1,u32> has_uv0;
	BitField<16,1,u32> has_uv1;
	BitField<17,1,u32> has_uv2;
	BitField<18,1,u32> has_uv3;
	BitField<19,1,u32> has_uv4;
	BitField<20,1,u32> has_uv5;
	BitField<21,1,u32> has_uv6;
	BitField<22,1,u32> has_uv7;

	// Convenience fields
	BitField< 2,8,u32> texmtxidxs;
	BitField<10,3,u32> normals;
	BitField<15,8,u32> uvs;

	u32 hex;

	inline void SetUv(int index, bool enabled = true)
	{
		if (index == 0) has_uv0 = enabled;
		else if (index == 1) has_uv1 = enabled;
		else if (index == 2) has_uv2 = enabled;
		else if (index == 3) has_uv3 = enabled;
		else if (index == 4) has_uv4 = enabled;
		else if (index == 5) has_uv5 = enabled;
		else if (index == 6) has_uv6 = enabled;
		else /*if (index == 7) */has_uv7 = enabled;
	}

	inline bool HasTexMtxIdx(int index) const
	{
		if (index == 0) return has_texmtxidx0;
		else if (index == 1) return has_texmtxidx1;
		else if (index == 2) return has_texmtxidx2;
		else if (index == 3) return has_texmtxidx3;
		else if (index == 4) return has_texmtxidx4;
		else if (index == 5) return has_texmtxidx5;
		else if (index == 6) return has_texmtxidx6;
		else /*if (index == 7) */return has_texmtxidx7;
	}

	inline bool HasUv(int index) const
	{
		if (index == 0) return has_uv0;
		else if (index == 1) return has_uv1;
		else if (index == 2) return has_uv2;
		else if (index == 3) return has_uv3;
		else if (index == 4) return has_uv4;
		else if (index == 5) return has_uv5;
		else if (index == 6) return has_uv6;
		else /*if (index == 7) */return has_uv7;
	}
};

#ifdef WIN32
#define LOADERDECL __cdecl
#else
#define LOADERDECL
#endif

typedef void (LOADERDECL *TPipelineFunction)();

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

	u32 GetVertexStride() const { return vertex_stride; }

	// TODO: move this under private:
	VertexComponents m_components;  // vertex components present

protected:
	// Let subclasses construct.
	NativeVertexFormat() {}

	u32 vertex_stride;
};
