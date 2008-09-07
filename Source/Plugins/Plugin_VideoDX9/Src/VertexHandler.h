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


#pragma once

#include "CPStructs.h"
#include "VertexLoader.h"
#include "DecodedVArray.h"

extern float shiftLookup[32];

struct UV
{
	float u,v,w;
};

struct D3DVertex {
	Vec3 pos;
	Vec3 normal;
	u32 colors[2];
	UV uv[8];
};

enum Collection
{
	C_NOTHING=0,
	C_TRIANGLES=1,
	C_LINES=2,
	C_POINTS=3
};
extern const Collection collectionTypeLUT[8];

class CVertexHandler
{
private:
	static Collection collection;
	// Pipeline

	static void PrepareRender();
	static void AddIndices(int _primitive, int _numVertices);

public:
	static void Init();
	static void Shutdown();

	static void BeginFrame();
	
	static void CreateDeviceObjects();
	static void DestroyDeviceObjects();

	static void DrawVertices(int _primitive, int _numVertices, const DecodedVArray *varray);
	static void Flush();
};
