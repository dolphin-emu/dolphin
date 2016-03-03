// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Common/CommonTypes.h"

class DataReader;
class NativeVertexFormat;
struct PortableVertexDeclaration;

namespace VertexLoaderManager
{
	using NativeVertexFormatMap = std::unordered_map<PortableVertexDeclaration, std::unique_ptr<NativeVertexFormat>>;

	void Init();
	void Shutdown();

	void MarkAllDirty();

	NativeVertexFormatMap* GetNativeVertexFormatMap();

	// Returns -1 if buf_size is insufficient, else the amount of bytes consumed
	int RunVertices(int vtx_attr_group, int primitive, int count, DataReader src, bool skip_drawing, bool is_preprocess);

	// For debugging
	void AppendListToString(std::string *dest);

	NativeVertexFormat* GetCurrentVertexFormat();

	// Resolved pointers to array bases. Used by vertex loaders.
	extern u8 *cached_arraybases[12];
	void UpdateVertexArrayPointers();

	// Position cache for zfreeze (3 vertices, 4 floats each to allow SIMD overwrite).
	// These arrays are in reverse order.
	extern float position_cache[3][4];
	extern u32 position_matrix_index[3];

	// VB_HAS_X. Bitmask telling what vertex components are present.
	extern u32 g_current_components;
}

