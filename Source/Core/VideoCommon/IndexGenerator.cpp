// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>

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

static u16* (*primitive_table[8])(u16*, u32, u32);

void IndexGenerator::Init()
{
  primitive_table[OpcodeDecoder::GX_DRAW_QUADS] = AddQuads;
  primitive_table[OpcodeDecoder::GX_DRAW_QUADS_2] = AddQuads_nonstandard;
  primitive_table[OpcodeDecoder::GX_DRAW_TRIANGLES] = AddList;
  primitive_table[OpcodeDecoder::GX_DRAW_TRIANGLE_STRIP] = AddStrip;
  primitive_table[OpcodeDecoder::GX_DRAW_TRIANGLE_FAN] = AddFan;
  primitive_table[OpcodeDecoder::GX_DRAW_LINES] = &AddLineList;
  primitive_table[OpcodeDecoder::GX_DRAW_LINE_STRIP] = &AddLineStrip;
  primitive_table[OpcodeDecoder::GX_DRAW_POINTS] = &AddPoints;
}

void IndexGenerator::Start(u16* Indexptr)
{
  index_buffer_current = Indexptr;
  BASEIptr = Indexptr;
  base_index = 0;
}

void IndexGenerator::AddIndices(int primitive, u32 numVerts)
{
  index_buffer_current = primitive_table[primitive](index_buffer_current, numVerts, base_index);
  base_index += numVerts;
}

u32 IndexGenerator::GetRemainingIndices()
{
  u32 max_index = 65534;  // -1 is reserved for primitive restart (ogl + dx11)
  return max_index - base_index;
}

//////////////////////////////////////////////////////////////////////////////////
// Triangles
u16* IndexGenerator::AddList(u16* Iptr, u32 const numVerts, u32 index)
{
  bool ccw = bpmem.genMode.cullmode == GenMode::CULL_FRONT;
  int v1 = ccw ? 2 : 1;
  int v2 = ccw ? 1 : 2;
  for (u32 i = 0; i < numVerts; i += 3)
  {
    *Iptr++ = index + i;
    *Iptr++ = index + i + v1;
    *Iptr++ = index + i + v2;
  }
  return Iptr;
}

u16* IndexGenerator::AddStrip(u16* Iptr, u32 const numVerts, u32 index)
{
  bool ccw = bpmem.genMode.cullmode == GenMode::CULL_FRONT;
  int wind = ccw ? 2 : 1;
  for (u32 i = 0; i < numVerts - 2; ++i)
  {
    *Iptr++ = index + i;
    *Iptr++ = index + i + wind;
    wind ^= 3;  // toggle between 1 and 2
    *Iptr++ = index + i + wind;
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
  int v1 = ccw ? 2 : 1;
  int v2 = ccw ? 1 : 2;
  for (u32 i = 0; i < numVerts - 2; ++i)
  {
    *Iptr++ = index;
    *Iptr++ = index + i + v1;
    *Iptr++ = index + i + v2;
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
  int v1 = ccw ? 2 : 1;
  int v2 = ccw ? 1 : 2;
  int v3 = ccw ? 3 : 2;
  int v4 = ccw ? 2 : 3;
  u32 i = 0;

  for (; i < (numVerts & ~3); i += 4)
  {
    *Iptr++ = index + i;
    *Iptr++ = index + i + v1;
    *Iptr++ = index + i + v2;

    *Iptr++ = index + i;
    *Iptr++ = index + i + v3;
    *Iptr++ = index + i + v4;
  }

  // Legend of Zelda The Wind Waker
  // if three vertices remaining, render a triangle
  if (numVerts & 3)
  {
    *Iptr++ = index + i;
    *Iptr++ = index + i + v1;
    *Iptr++ = index + i + v2;
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
  for (u32 i = 0; i < numVerts; i += 2)
  {
    *Iptr++ = index + i;
    *Iptr++ = index + i + 1;
  }
  return Iptr;
}

// shouldn't be used as strips as LineLists are much more common
// so converting them to lists
u16* IndexGenerator::AddLineStrip(u16* Iptr, u32 numVerts, u32 index)
{
  for (u32 i = 0; i < numVerts - 1; ++i)
  {
    *Iptr++ = index + i;
    *Iptr++ = index + i + 1;
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

