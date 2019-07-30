// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Compiler.h"
#include "Common/Logging/Log.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/BPMemory.h"

// Init
u16* IndexGenerator::index_buffer_current;
u16* IndexGenerator::BASEIptr;
u32 IndexGenerator::base_index;

void IndexGenerator::Start(u16* Indexptr)
{
  index_buffer_current = Indexptr;
  BASEIptr = Indexptr;
  base_index = 0;
}

void IndexGenerator::AddIndices(int primitive, u32 numVerts)
{
  switch (primitive)
  {
  case OpcodeDecoder::GX_DRAW_QUADS:
    index_buffer_current = AddQuads(index_buffer_current, numVerts, base_index);
    break;
  case OpcodeDecoder::GX_DRAW_QUADS_2:
    index_buffer_current = AddQuads_nonstandard(index_buffer_current, numVerts, base_index);
    break;
  case OpcodeDecoder::GX_DRAW_TRIANGLES:
    index_buffer_current = AddList(index_buffer_current, numVerts, base_index);
    break;
  case OpcodeDecoder::GX_DRAW_TRIANGLE_STRIP:
    index_buffer_current = AddStrip(index_buffer_current, numVerts, base_index);
    break;
  case OpcodeDecoder::GX_DRAW_TRIANGLE_FAN:
    index_buffer_current = AddFan(index_buffer_current, numVerts, base_index);
    break;
  case OpcodeDecoder::GX_DRAW_LINES:
    index_buffer_current = AddLineList(index_buffer_current, numVerts, base_index);
    break;
  case OpcodeDecoder::GX_DRAW_LINE_STRIP:
    index_buffer_current = AddLineStrip(index_buffer_current, numVerts, base_index);
    break;
  case OpcodeDecoder::GX_DRAW_POINTS:
    index_buffer_current = AddPoints(index_buffer_current, numVerts, base_index);
    break;
  }
  base_index += numVerts;
}

void IndexGenerator::AddExternalIndices(const u16* indices, u32 num_indices, u32 num_vertices)
{
  std::memcpy(index_buffer_current, indices, sizeof(u16) * num_indices);
  index_buffer_current += num_indices;
  base_index += num_vertices;
}

// Triangles
u16* IndexGenerator::AddList(u16* Iptr, u32 numVerts, u32 index)
{
  bool ccw = bpmem.genMode.cullmode == GenMode::CULL_FRONT;
  int v1 = ccw ? 0 : 1;
  int v2 = ccw ? 1 : 0;
  for (u32 i = 2; i < numVerts; i += 3)
  {
    *Iptr++ = index + i - 2;
    *Iptr++ = index + i - v1;
    *Iptr++ = index + i - v2;
  }
  return Iptr;
}

u16* IndexGenerator::AddStrip(u16* Iptr, u32 numVerts, u32 index)
{
  bool ccw = bpmem.genMode.cullmode == GenMode::CULL_FRONT;
  int wind = ccw ? 0 : 1;
  for (u32 i = 2; i < numVerts; ++i)
  {
    *Iptr++ = index + i - 2;
    *Iptr++ = index + i - wind;
    wind ^= 1;  // toggle between 0 and 1
    *Iptr++ = index + i - wind;
  }
  return Iptr;
}

/**
 * FAN simulator:
 *
 *   2---3
 *  / \ / \
 * 1---0---4
 *
 * would generate this triangles:
 * 012, 023, 034
 *
 * rotated (for better striping):
 * 120, 302, 034
 *
 * as odd ones have to winded, following strip is fine:
 * 12034
 *
 * so we use 6 indices for 3 triangles
 */
u16* IndexGenerator::AddFan(u16* Iptr, u32 numVerts, u32 index)
{
  bool ccw = bpmem.genMode.cullmode == GenMode::CULL_FRONT;
  int v1 = ccw ? 0 : 1;
  int v2 = ccw ? 1 : 0;
  for (u32 i = 2; i < numVerts; ++i)
  {
    *Iptr++ = index;
    *Iptr++ = index + i - v1;
    *Iptr++ = index + i - v2;
  }
  return Iptr;
}

/*
 * QUAD simulator
 *
 * 0---1   4---5
 * |\  |   |\  |
 * | \ |   | \ |
 * |  \|   |  \|
 * 3---2   7---6
 *
 * 012,023, 456,467 ...
 * or 120,302, 564,746
 * or as strip: 1203, 5647
 *
 * Warning:
 * A simple triangle has to be rendered for three vertices.
 * ZWW do this for sun rays
 */
u16* IndexGenerator::AddQuads(u16* Iptr, u32 numVerts, u32 index)
{
  bool ccw = bpmem.genMode.cullmode == GenMode::CULL_FRONT;
  u32 i = 3;
  int v1 = ccw ? 1 : 2;
  int v2 = ccw ? 2 : 1;
  int v3 = ccw ? 0 : 1;
  int v4 = ccw ? 1 : 0;

  for (; i < numVerts; i += 4)
  {
    *Iptr++ = index + i - 3;
    *Iptr++ = index + i - v1;
    *Iptr++ = index + i - v2;

    *Iptr++ = index + i - 3;
    *Iptr++ = index + i - v3;
    *Iptr++ = index + i - v4;
  }

  // Legend of Zelda The Wind Waker
  // if three vertices remaining, render a triangle
  if (i == numVerts)
  {
    *Iptr++ = index + i - 3;
    *Iptr++ = index + i - v1;
    *Iptr++ = index + i - v2;
  }

  return Iptr;
}

u16* IndexGenerator::AddQuads_nonstandard(u16* Iptr, u32 numVerts, u32 index)
{
  WARN_LOG(VIDEO, "Non-standard primitive drawing command GL_DRAW_QUADS_2");
  return AddQuads(Iptr, numVerts, index);
}

// Lines
u16* IndexGenerator::AddLineList(u16* Iptr, u32 numVerts, u32 index)
{
  for (u32 i = 1; i < numVerts; i += 2)
  {
    *Iptr++ = index + i - 1;
    *Iptr++ = index + i;
  }
  return Iptr;
}

// shouldn't be used as strips as LineLists are much more common
// so converting them to lists
u16* IndexGenerator::AddLineStrip(u16* Iptr, u32 numVerts, u32 index)
{
  for (u32 i = 1; i < numVerts; ++i)
  {
    *Iptr++ = index + i - 1;
    *Iptr++ = index + i;
  }
  return Iptr;
}

// Points
u16* IndexGenerator::AddPoints(u16* Iptr, u32 numVerts, u32 index)
{
  for (u32 i = 0; i != numVerts; ++i)
  {
    *Iptr++ = index + i;
  }
  return Iptr;
}
