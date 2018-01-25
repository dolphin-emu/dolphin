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
using NativeVertexFormatMap =
    std::unordered_map<PortableVertexDeclaration, std::unique_ptr<NativeVertexFormat>>;

void Init();
void Clear();

void MarkAllDirty();

// Creates or obtains a pointer to a VertexFormat representing decl.
// If this results in a VertexFormat being created, if the game later uses a matching vertex
// declaration, the one that was previously created will be used.
NativeVertexFormat* GetOrCreateMatchingFormat(const PortableVertexDeclaration& decl);

// For vertex ubershaders, all attributes need to be present, even when the vertex
// format does not contain them. This function returns a vertex format with dummy
// offsets set to the unused attributes.
NativeVertexFormat* GetUberVertexFormat(const PortableVertexDeclaration& decl);

// Returns -1 if buf_size is insufficient, else the amount of bytes consumed
int RunVertices(int vtx_attr_group, int primitive, int count, DataReader src, bool skip_drawing,
                bool is_preprocess);

// For debugging
std::string VertexLoadersToString();

NativeVertexFormat* GetCurrentVertexFormat();

// Resolved pointers to array bases. Used by vertex loaders.
extern u8* cached_arraybases[12];
void UpdateVertexArrayPointers();

// Position cache for zfreeze (3 vertices, 4 floats each to allow SIMD overwrite).
// These arrays are in reverse order.
extern float position_cache[3][4];
extern u32 position_matrix_index[4];

// VB_HAS_X. Bitmask telling what vertex components are present.
extern u32 g_current_components;
}
