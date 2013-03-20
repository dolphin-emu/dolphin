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

// This is currently only used by the DX backend, but it may make sense to
// use it in the GL backend or a future DX10 backend too.

#ifndef _INDEXGENERATOR_H
#define _INDEXGENERATOR_H
#include "CommonTypes.h"

class IndexGenerator
{
public:
	// Init
	static void Start(u16 *Triangleptr,u16 *Lineptr,u16 *Pointptr);

	static void AddIndices(int primitive, u32 numVertices);

	// Interface
	static u32 GetNumTriangles() {return numT;}
	static u32 GetNumLines() {return numL;}
	static u32 GetNumPoints() {return numP;}

	// returns numprimitives
	static u32 GetNumVerts() {return index;}

	static u32 GetTriangleindexLen() {return (u32)(Tptr - BASETptr);}
	static u32 GetLineindexLen() {return (u32)(Lptr - BASELptr);}
	static u32 GetPointindexLen() {return (u32)(Pptr - BASEPptr);}
/*
	enum IndexPrimitiveType
	{
		Prim_None = 0,
		Prim_List,
		Prim_Strip,
		Prim_Fan
	};
*/
private:
	// Triangles
	static void AddList(u32 numVerts);
	static void AddStrip(u32 numVerts);
	static void AddFan(u32 numVerts);
	static void AddQuads(u32 numVerts);

	// Lines
	static void AddLineList(u32 numVerts);
	static void AddLineStrip(u32 numVerts);

	// Points
	static void AddPoints(u32 numVerts);

	static void WriteTriangle(u32 index1, u32 index2, u32 index3);

	static u16 *Tptr;
	static u16 *BASETptr;
	static u16 *Lptr;
	static u16 *BASELptr;
	static u16 *Pptr;
	static u16 *BASEPptr;
	// TODO: redundant variables
	static u32 numT;
	static u32 numL;
	static u32 numP;
	static u32 index;
};

#endif  // _INDEXGENERATOR_H
