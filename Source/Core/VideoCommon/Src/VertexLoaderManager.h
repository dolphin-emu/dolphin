// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _VERTEXLOADERMANAGER_H
#define _VERTEXLOADERMANAGER_H

#include "Common.h"
#include <string>

namespace VertexLoaderManager
{
	void Init();
	void Shutdown();

	void MarkAllDirty();

	int GetVertexSize(int vtx_attr_group);
	void RunVertices(int vtx_attr_group, int primitive, int count);
	void RunCompiledVertices(int vtx_attr_group, int primitive, int count, u8* Data);

	// For debugging
	void AppendListToString(std::string *dest);
};

void RecomputeCachedArraybases();

#endif  // _VERTEXLOADERMANAGER_H
