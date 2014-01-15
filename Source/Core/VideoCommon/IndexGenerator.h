// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// This is currently only used by the DX backend, but it may make sense to
// use it in the GL backend or a future DX10 backend too.

#ifndef _INDEXGENERATOR_H
#define _INDEXGENERATOR_H
#include "CommonTypes.h"

class IndexGenerator
{
public:
	// Init
	static void Init();
	static void Start(u16 *Indexptr);

	static void AddIndices(int primitive, u32 numVertices);

	// Interface
	static u32 GetNumIndices() {return numI;}

	// returns numprimitives
	static u32 GetNumVerts() {return index;}

	static u32 GetIndexLen() {return (u32)(Iptr - BASEIptr);}

	static u32 GetRemainingIndices();
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
	template <bool pr> static void AddList(u32 numVerts);
	template <bool pr> static void AddStrip(u32 numVerts);
	template <bool pr> static void AddFan(u32 numVerts);
	template <bool pr> static void AddQuads(u32 numVerts);

	// Lines
	static void AddLineList(u32 numVerts);
	static void AddLineStrip(u32 numVerts);

	// Points
	static void AddPoints(u32 numVerts);

	template <bool pr> static void WriteTriangle(u32 index1, u32 index2, u32 index3);

	static u16 *Iptr;
	static u16 *BASEIptr;
	// TODO: redundant variables
	static u32 numI;
	static u32 index;
};

#endif  // _INDEXGENERATOR_H
