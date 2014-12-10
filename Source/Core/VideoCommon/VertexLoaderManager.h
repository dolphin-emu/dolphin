// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/NativeVertexFormat.h"

namespace VertexLoaderManager
{
	void Init();
	void Shutdown();

	void MarkAllDirty();

	int GetVertexSize(int vtx_attr_group, bool preprocess);

	// Returns -1 if buf_size is insufficient, else the amount of bytes consumed
	int RunVertices(int vtx_attr_group, int primitive, int count, DataReader src, bool skip_drawing = false);

	// For debugging
	void AppendListToString(std::string *dest);

	NativeVertexFormat* GetCurrentVertexFormat();
}

void RecomputeCachedArraybases();
