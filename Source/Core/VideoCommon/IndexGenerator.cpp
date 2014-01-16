// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>

#include "Common.h"
#include "VideoConfig.h"
#include "IndexGenerator.h"

//Init
u16 *IndexGenerator::Iptr;
u16 *IndexGenerator::BASEIptr;
u32 IndexGenerator::index;

static const u16 s_primitive_restart = -1;

static void (*primitive_table[8])(u32);

void IndexGenerator::Init()
{
	if(g_Config.backend_info.bSupportsPrimitiveRestart)
	{
		primitive_table[0] = IndexGenerator::AddQuads<true>;
		primitive_table[2] = IndexGenerator::AddList<true>;
		primitive_table[3] = IndexGenerator::AddStrip<true>;
		primitive_table[4] = IndexGenerator::AddFan<true>;
	}
	else
	{
		primitive_table[0] = IndexGenerator::AddQuads<false>;
		primitive_table[2] = IndexGenerator::AddList<false>;
		primitive_table[3] = IndexGenerator::AddStrip<false>;
		primitive_table[4] = IndexGenerator::AddFan<false>;
	}
	primitive_table[1] = NULL;
	primitive_table[5] = &IndexGenerator::AddLineList;
	primitive_table[6] = &IndexGenerator::AddLineStrip;
	primitive_table[7] = &IndexGenerator::AddPoints;
}

void IndexGenerator::Start(u16* Indexptr)
{
	Iptr = Indexptr;
	BASEIptr = Indexptr;
	index = 0;
}

void IndexGenerator::AddIndices(int primitive, u32 numVerts)
{
	primitive_table[primitive](numVerts);
	index += numVerts;
}

// Triangles
template <bool pr> __forceinline void IndexGenerator::WriteTriangle(u32 index1, u32 index2, u32 index3)
{
	*Iptr++ = index1;
	*Iptr++ = index2;
	*Iptr++ = index3;
	if(pr)
		*Iptr++ = s_primitive_restart;
}

template <bool pr> void IndexGenerator::AddList(u32 const numVerts)
{
	for (u32 i = 2; i < numVerts; i+=3)
	{
		WriteTriangle<pr>(index + i - 2, index + i - 1, index + i);
	}
}

template <bool pr> void IndexGenerator::AddStrip(u32 const numVerts)
{
	if(pr)
	{
		for (u32 i = 0; i < numVerts; ++i)
		{
			*Iptr++ = index + i;
		}
		*Iptr++ = s_primitive_restart;

	}
	else
	{
		bool wind = false;
		for (u32 i = 2; i < numVerts; ++i)
		{
			WriteTriangle<pr>(
				index + i - 2,
				index + i - !wind,
				index + i - wind);

			wind ^= true;
		}
	}
}

/**
 * FAN simulator:
 *
 *   2---3
 *  / \ / \
 * 1---0---4
 *
 * would generate this triangles:
 * 012, 023, 034
 *
 * rotated (for better striping):
 * 120, 302, 034
 *
 * as odd ones have to winded, following strip is fine:
 * 12034
 *
 * so we use 6 indices for 3 triangles
 */

template <bool pr> void IndexGenerator::AddFan(u32 numVerts)
{
	u32 i = 2;

	if(pr)
	{
		for(; i+3<=numVerts; i+=3)
		{
			*Iptr++ = index + i - 1;
			*Iptr++ = index + i + 0;
			*Iptr++ = index;
			*Iptr++ = index + i + 1;
			*Iptr++ = index + i + 2;
			*Iptr++ = s_primitive_restart;
		}

		for(; i+2<=numVerts; i+=2)
		{
			*Iptr++ = index + i - 1;
			*Iptr++ = index + i + 0;
			*Iptr++ = index;
			*Iptr++ = index + i + 1;
			*Iptr++ = s_primitive_restart;
		}
	}

	for (; i < numVerts; ++i)
	{
		WriteTriangle<pr>(index, index + i - 1, index + i);
	}
}

/*
 * QUAD simulator
 *
 * 0---1   4---5
 * |\  |   |\  |
 * | \ |   | \ |
 * |  \|   |  \|
 * 3---2   7---6
 *
 * 012,023, 456,467 ...
 * or 120,302, 564,746
 * or as strip: 1203, 5647
 *
 * Warning:
 * A simple triangle has to be rendered for three vertices.
 * ZWW do this for sun rays
 */
template <bool pr> void IndexGenerator::AddQuads(u32 numVerts)
{
	u32 i = 3;
	for (; i < numVerts; i+=4)
	{
		if(pr)
		{
			*Iptr++ = index + i - 2;
			*Iptr++ = index + i - 1;
			*Iptr++ = index + i - 3;
			*Iptr++ = index + i - 0;
			*Iptr++ = s_primitive_restart;
		}
		else
		{
			WriteTriangle<pr>(index + i - 3, index + i - 2, index + i - 1);
			WriteTriangle<pr>(index + i - 3, index + i - 1, index + i - 0);
		}
	}

	// three vertices remaining, so render a triangle
	if(i == numVerts)
	{
		WriteTriangle<pr>(index+numVerts-3, index+numVerts-2, index+numVerts-1);
	}
}

// Lines
void IndexGenerator::AddLineList(u32 numVerts)
{
	for (u32 i = 1; i < numVerts; i+=2)
	{
		*Iptr++ = index + i - 1;
		*Iptr++ = index + i;
	}

}

// shouldn't be used as strips as LineLists are much more common
// so converting them to lists
void IndexGenerator::AddLineStrip(u32 numVerts)
{
	for (u32 i = 1; i < numVerts; ++i)
	{
		*Iptr++ = index + i - 1;
		*Iptr++ = index + i;
	}
}

// Points
void IndexGenerator::AddPoints(u32 numVerts)
{
	for (u32 i = 0; i != numVerts; ++i)
	{
		*Iptr++ = index + i;
	}
}


u32 IndexGenerator::GetRemainingIndices()
{
	u32 max_index = 65534; // -1 is reserved for primitive restart (ogl + dx11)
	return max_index - index;
}
