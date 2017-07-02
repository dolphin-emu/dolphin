// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VideoConfig.h"

// Init
u16* IndexGenerator::index_buffer_current;
u16* IndexGenerator::BASEIptr;
u32 IndexGenerator::base_index;

static const u16 s_primitive_restart = UINT16_MAX;

static u16* (*primitive_table[8])(u16*, u32, u32);

void IndexGenerator::Init()
{
  if (g_Config.backend_info.bSupportsPrimitiveRestart)
  {
    primitive_table[OpcodeDecoder::GX_DRAW_QUADS] = AddQuads<true>;
    primitive_table[OpcodeDecoder::GX_DRAW_QUADS_2] = AddQuads_nonstandard<true>;
    primitive_table[OpcodeDecoder::GX_DRAW_TRIANGLES] = AddList<true>;
    primitive_table[OpcodeDecoder::GX_DRAW_TRIANGLE_STRIP] = AddStrip<true>;
    primitive_table[OpcodeDecoder::GX_DRAW_TRIANGLE_FAN] = AddFan<true>;
  }
  else
  {
    primitive_table[OpcodeDecoder::GX_DRAW_QUADS] = AddQuads<false>;
    primitive_table[OpcodeDecoder::GX_DRAW_QUADS_2] = AddQuads_nonstandard<false>;
    primitive_table[OpcodeDecoder::GX_DRAW_TRIANGLES] = AddList<false>;
    primitive_table[OpcodeDecoder::GX_DRAW_TRIANGLE_STRIP] = AddStrip<false>;
    primitive_table[OpcodeDecoder::GX_DRAW_TRIANGLE_FAN] = AddFan<false>;
  }
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

// Triangles
template <bool pr>
__forceinline u16* IndexGenerator::WriteTriangle(u16* Iptr, u32 index1, u32 index2, u32 index3)
{
  *Iptr++ = index1;
  *Iptr++ = index2;
  *Iptr++ = index3;
  if (pr)
    *Iptr++ = s_primitive_restart;
  return Iptr;
}

template <bool pr>
u16* IndexGenerator::AddList(u16* Iptr, u32 const numVerts, u32 index)
{
  for (u32 i = 2; i < numVerts; i += 3)
  {
    Iptr = WriteTriangle<pr>(Iptr, index + i - 2, index + i - 1, index + i);
  }
  return Iptr;
}

template <bool pr>
u16* IndexGenerator::AddStrip(u16* Iptr, u32 const numVerts, u32 index)
{
  if (pr)
  {
    for (u32 i = 0; i < numVerts; ++i)
    {
      *Iptr++ = index + i;
    }
    *Iptr++ = s_primitive_restart;
  }
  else
  {
    bool wind = false;
    for (u32 i = 2; i < numVerts; ++i)
    {
      Iptr = WriteTriangle<pr>(Iptr, index + i - 2, index + i - !wind, index + i - wind);

      wind ^= true;
    }
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

template <bool pr>
u16* IndexGenerator::AddFan(u16* Iptr, u32 numVerts, u32 index)
{
  u32 i = 2;

  if (pr)
  {
    for (; i + 3 <= numVerts; i += 3)
    {
      *Iptr++ = index + i - 1;
      *Iptr++ = index + i + 0;
      *Iptr++ = index;
      *Iptr++ = index + i + 1;
      *Iptr++ = index + i + 2;
      *Iptr++ = s_primitive_restart;
    }

    for (; i + 2 <= numVerts; i += 2)
    {
      *Iptr++ = index + i - 1;
      *Iptr++ = index + i + 0;
      *Iptr++ = index;
      *Iptr++ = index + i + 1;
      *Iptr++ = s_primitive_restart;
    }
  }

  for (; i < numVerts; ++i)
  {
    Iptr = WriteTriangle<pr>(Iptr, index, index + i - 1, index + i);
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
template <bool pr>
u16* IndexGenerator::AddQuads(u16* Iptr, u32 numVerts, u32 index)
{
  u32 i = 3;
  for (; i < numVerts; i += 4)
  {
    if (pr)
    {
      *Iptr++ = index + i - 2;
      *Iptr++ = index + i - 1;
      *Iptr++ = index + i - 3;
      *Iptr++ = index + i - 0;
      *Iptr++ = s_primitive_restart;
    }
    else
    {
      Iptr = WriteTriangle<pr>(Iptr, index + i - 3, index + i - 2, index + i - 1);
      Iptr = WriteTriangle<pr>(Iptr, index + i - 3, index + i - 1, index + i - 0);
    }
  }

  // three vertices remaining, so render a triangle
  if (i == numVerts)
  {
    Iptr =
        WriteTriangle<pr>(Iptr, index + numVerts - 3, index + numVerts - 2, index + numVerts - 1);
  }
  return Iptr;
}

template <bool pr>
u16* IndexGenerator::AddQuads_nonstandard(u16* Iptr, u32 numVerts, u32 index)
{
  WARN_LOG(VIDEO, "Non-standard primitive drawing command GL_DRAW_QUADS_2");
  return AddQuads<pr>(Iptr, numVerts, index);
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

u32 IndexGenerator::GetRemainingIndices()
{
  u32 max_index = 65534;  // -1 is reserved for primitive restart (ogl + dx11)
  return max_index - base_index;
}
