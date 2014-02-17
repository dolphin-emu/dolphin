// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Common.h"

namespace VertexLoaderManager
{
	void Init();
	void Shutdown();

	void MarkAllDirty();

	int GetVertexSize(int vtx_attr_group);
	void RunVertices(int vtx_attr_group, int primitive, int count);

	// For debugging
	void AppendListToString(std::string *dest);
};

void RecomputeCachedArraybases();
