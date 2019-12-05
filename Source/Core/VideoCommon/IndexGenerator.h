// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This is currently only used by the DX backend, but it may make sense to
// use it in the GL backend or a future DX10 backend too.

#pragma once

#include "Common/CommonTypes.h"

class IndexGenerator
{
public:
  // Init
  static void Init();
  static void Start(u16* Indexptr);

  static void AddIndices(int primitive, u32 numVertices);

  static void AddExternalIndices(const u16* indices, u32 num_indices, u32 num_vertices);

  // returns numprimitives
  static u32 GetNumVerts() { return base_index; }
  static u32 GetIndexLen() { return (u32)(index_buffer_current - BASEIptr); }
  static u32 GetRemainingIndices();

private:
  static u16* index_buffer_current;
  static u16* BASEIptr;
  static u32 base_index;
};
