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

#include "VideoCommon.h"
#include "VertexLoader.h"
#include "VertexManager.h"

DecodedVArray tempvarray;

namespace VertexLoaderManager
{

void Init()
{
	tempvarray.Create(65536*3, 1, 8, 3, 2, 8);
}

void Shutdown()
{
	tempvarray.Destroy();
}

int GetVertexSize(int vat)
{
	VertexLoader& vtxLoader = g_VertexLoaders[vat];
	vtxLoader.Setup();
	return vtxLoader.GetVertexSize();
}

void RunVertices(int vat, int primitive, int num_vertices)
{
	tempvarray.Reset();
	VertexLoader::SetVArray(&tempvarray);
	VertexLoader& vtxLoader = g_VertexLoaders[vat];
	vtxLoader.Setup();
	vtxLoader.PrepareRun();
	int vsize = vtxLoader.GetVertexSize();
	vtxLoader.RunVertices(num_vertices);
	VertexManager::AddVertices(primitive, num_vertices, &tempvarray);
}

}