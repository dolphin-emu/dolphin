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

#include <cstddef>

#include "IndexGenerator.h"

/*
*
QUAD simulator

0   1   4   5
3   2   7   6
012023 147172 ...
*/

//Init
u16 *IndexGenerator::Tptr = 0;
u16 *IndexGenerator::BASETptr = 0;
u16 *IndexGenerator::Lptr = 0;
u16 *IndexGenerator::BASELptr = 0;
u16 *IndexGenerator::Pptr = 0;
u16 *IndexGenerator::BASEPptr = 0;
u32 IndexGenerator::numT = 0;
u32 IndexGenerator::numL = 0;
u32 IndexGenerator::numP = 0;
u32 IndexGenerator::index = 0;

void IndexGenerator::Start(u16* Triangleptr, u16* Lineptr, u16* Pointptr)
{
	Tptr = Triangleptr;
	Lptr = Lineptr;
	Pptr = Pointptr;
	BASETptr = Triangleptr;
	BASELptr = Lineptr;
	BASEPptr = Pointptr;
	index = 0;
	numT = 0;
	numL = 0;
	numP = 0;
}

void IndexGenerator::AddIndices(int primitive, u32 numVerts)
{
	//switch (primitive)
	//{
	//case GX_DRAW_QUADS:          IndexGenerator::AddQuads(numVertices);		break;
	//case GX_DRAW_TRIANGLES:      IndexGenerator::AddList(numVertices);		break;
	//case GX_DRAW_TRIANGLE_STRIP: IndexGenerator::AddStrip(numVertices);		break;
	//case GX_DRAW_TRIANGLE_FAN:   IndexGenerator::AddFan(numVertices);		break;
	//case GX_DRAW_LINES:		   IndexGenerator::AddLineList(numVertices);	break;
	//case GX_DRAW_LINE_STRIP:     IndexGenerator::AddLineStrip(numVertices);	break;
	//case GX_DRAW_POINTS:         IndexGenerator::AddPoints(numVertices);	break;
	//}

	static void (*const primitive_table[])(u32) =
	{
		IndexGenerator::AddQuads,
		NULL,
		IndexGenerator::AddList,
		IndexGenerator::AddStrip,
		IndexGenerator::AddFan,
		IndexGenerator::AddLineList,
		IndexGenerator::AddLineStrip,
		IndexGenerator::AddPoints,
	};

	primitive_table[primitive](numVerts);
	index += numVerts;
}

// Triangles
void IndexGenerator::WriteTriangle(u32 index1, u32 index2, u32 index3)
{
	*Tptr++ = index1;
	*Tptr++ = index2;
	*Tptr++ = index3;
	
	++numT;
}

void IndexGenerator::AddList(u32 const numVerts)
{	
	auto const numTris = numVerts / 3;
	if (!numTris)
	{
		if (2 == numVerts)
		{
			// We have two verts. Render a degenerated triangle.
			WriteTriangle(index, index + 1, index);
		}
	}
	else
	{
		for (u32 i = 0; i != numTris; ++i)
		{
			WriteTriangle(index + i * 3, index + i * 3 + 1, index + i * 3 + 2);
		}
		
		auto const base_remaining_verts = numTris * 3;
		switch (numVerts % 3)
		{
		case 2:
			// We have 2 remaining verts. Use strip method
			WriteTriangle(
				index + base_remaining_verts - 1,
				index + base_remaining_verts,
				index + base_remaining_verts + 1);
			
			break;
			
		case 1:
			// We have 1 remaining vert. Use strip method this is only a conjeture
			WriteTriangle(
				index + base_remaining_verts - 2,
				index + base_remaining_verts - 1,
				index + base_remaining_verts);
			break;
			
		default:
			break;
		};
	}
}

void IndexGenerator::AddStrip(u32 const numVerts)
{
	if (numVerts < 3) 
	{
		if (2 == numVerts)
		{
			// We have two verts. Render a degenerated triangle.
			WriteTriangle(index, index + 1, index);
		}
	}
	else
	{
		bool wind = false;
		for (u32 i = 2; i < numVerts; ++i)
		{
			WriteTriangle(
				index + i - 2,
				index + i - !wind,
				index + i - wind);
			
			wind ^= true;
		}
	}
}

void IndexGenerator::AddFan(u32 numVerts)
{
	if (numVerts < 3)
	{
		if (2 == numVerts)
		{
			// We have two verts. Render a degenerated triangle.
			WriteTriangle(index, index + 1, index);
		}
	}
	else
	{
		for (u32 i = 2; i < numVerts; ++i)
		{
			WriteTriangle(index, index + i - 1, index + i);
		}
	}
}

void IndexGenerator::AddQuads(u32 numVerts)
{
	auto const numQuads = numVerts / 4;
	if (!numQuads)
	{
		if (2 == numVerts)
		{
			// We have two verts. Render a degenerated triangle.
			WriteTriangle(index, index + 1, index);
		}
		else if (3 == numVerts);
		{
			// We have 3 verts. Render a full triangle.
			WriteTriangle(index, index + 1, index + 2);
		}
	}
	else
	{
		for (u32 i = 0; i != numQuads; ++i)
		{
			WriteTriangle(index + i * 4, index + i * 4 + 1, index + i * 4 + 2);
			WriteTriangle(index + i * 4, index + i * 4 + 2, index + i * 4 + 3);
		}
		
		auto const base_remaining_verts = numQuads * 4;
		switch (numVerts % 4)
		{
		case 3:
			// We have 3 remaining verts. Use strip method.
			WriteTriangle(
				index + base_remaining_verts,
				index + base_remaining_verts + 1,
				index + base_remaining_verts + 2);
			break;
			
		case 2:
			// We have 3 remaining verts. Use strip method.
			WriteTriangle(
				index + base_remaining_verts - 1,
				index + base_remaining_verts,
				index + base_remaining_verts + 1);
			break;
			
		case 1:
			// We have 1 remaining verts use strip method. This is only a conjeture.
			WriteTriangle(
				base_remaining_verts - 2,
				index + base_remaining_verts - 1,
				index + base_remaining_verts);
			break;
			
		default:
			break;
		};
	}
}

// Lines
void IndexGenerator::AddLineList(u32 numVerts)
{
	auto const numLines = numVerts / 2;
	for (u32 i = 0; i != numLines; ++i)
	{
		*Lptr++ = index + i * 2;
		*Lptr++ = index + i * 2 + 1;
		++numL;
	}
}

void IndexGenerator::AddLineStrip(u32 numVerts)
{
	for (u32 i = 1; i < numVerts; ++i)
	{
		*Lptr++ = index + i - 1;
		*Lptr++ = index + i;
		++numL;
	}
}

// Points
void IndexGenerator::AddPoints(u32 numVerts)
{
	for (u32 i = 0; i != numVerts; ++i)
	{
		*Pptr++ = index + i;
		++numP;
	}
}
