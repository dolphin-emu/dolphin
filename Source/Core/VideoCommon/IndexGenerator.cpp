// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/IndexGenerator.h"

#include <array>
#include <cstddef>
#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
constexpr u16 s_primitive_restart = UINT16_MAX;

template <bool pr>
u16* WriteTriangle(u16* index_ptr, u32 index1, u32 index2, u32 index3)
{
  *index_ptr++ = index1;
  *index_ptr++ = index2;
  *index_ptr++ = index3;
  if constexpr (pr)
    *index_ptr++ = s_primitive_restart;
  return index_ptr;
}

template <bool pr>
u16* AddList(u16* index_ptr, u32 num_verts, u32 index)
{
  for (u32 i = 2; i < num_verts; i += 3)
  {
    index_ptr = WriteTriangle<pr>(index_ptr, index + i - 2, index + i - 1, index + i);
  }
  return index_ptr;
}

template <bool pr>
u16* AddStrip(u16* index_ptr, u32 num_verts, u32 index)
{
  if constexpr (pr)
  {
    for (u32 i = 0; i < num_verts; ++i)
    {
      *index_ptr++ = index + i;
    }
    *index_ptr++ = s_primitive_restart;
  }
  else
  {
    bool wind = false;
    for (u32 i = 2; i < num_verts; ++i)
    {
      index_ptr = WriteTriangle<pr>(index_ptr, index + i - 2, index + i - !wind, index + i - wind);

      wind ^= true;
    }
  }
  return index_ptr;
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
u16* AddFan(u16* index_ptr, u32 num_verts, u32 index)
{
  u32 i = 2;

  if constexpr (pr)
  {
    for (; i + 3 <= num_verts; i += 3)
    {
      *index_ptr++ = index + i - 1;
      *index_ptr++ = index + i + 0;
      *index_ptr++ = index;
      *index_ptr++ = index + i + 1;
      *index_ptr++ = index + i + 2;
      *index_ptr++ = s_primitive_restart;
    }

    for (; i + 2 <= num_verts; i += 2)
    {
      *index_ptr++ = index + i - 1;
      *index_ptr++ = index + i + 0;
      *index_ptr++ = index;
      *index_ptr++ = index + i + 1;
      *index_ptr++ = s_primitive_restart;
    }
  }

  for (; i < num_verts; ++i)
  {
    index_ptr = WriteTriangle<pr>(index_ptr, index, index + i - 1, index + i);
  }
  return index_ptr;
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
u16* AddQuads(u16* index_ptr, u32 num_verts, u32 index)
{
  u32 i = 3;
  for (; i < num_verts; i += 4)
  {
    if constexpr (pr)
    {
      *index_ptr++ = index + i - 2;
      *index_ptr++ = index + i - 1;
      *index_ptr++ = index + i - 3;
      *index_ptr++ = index + i - 0;
      *index_ptr++ = s_primitive_restart;
    }
    else
    {
      index_ptr = WriteTriangle<pr>(index_ptr, index + i - 3, index + i - 2, index + i - 1);
      index_ptr = WriteTriangle<pr>(index_ptr, index + i - 3, index + i - 1, index + i - 0);
    }
  }

  // three vertices remaining, so render a triangle
  if (i == num_verts)
  {
    index_ptr = WriteTriangle<pr>(index_ptr, index + num_verts - 3, index + num_verts - 2,
                                  index + num_verts - 1);
  }
  return index_ptr;
}

template <bool pr>
u16* AddQuads_nonstandard(u16* index_ptr, u32 num_verts, u32 index)
{
  WARN_LOG_FMT(VIDEO, "Non-standard primitive drawing command GL_DRAW_QUADS_2");
  return AddQuads<pr>(index_ptr, num_verts, index);
}

u16* AddLineList(u16* index_ptr, u32 num_verts, u32 index)
{
  for (u32 i = 1; i < num_verts; i += 2)
  {
    *index_ptr++ = index + i - 1;
    *index_ptr++ = index + i;
  }
  return index_ptr;
}

// Shouldn't be used as strips as LineLists are much more common
// so converting them to lists
u16* AddLineStrip(u16* index_ptr, u32 num_verts, u32 index)
{
  for (u32 i = 1; i < num_verts; ++i)
  {
    *index_ptr++ = index + i - 1;
    *index_ptr++ = index + i;
  }
  return index_ptr;
}

template <bool pr, bool linestrip>
u16* AddLines_VSExpand(u16* index_ptr, u32 num_verts, u32 index)
{
  // VS Expand uses (index >> 2) as the base vertex
  // Bit 0 indicates which side of the line (left/right for a vertical line)
  // Bit 1 indicates which point of the line (top/bottom for a vertical line)
  // VS Expand assumes the two points will be adjacent vertices
  constexpr u32 advance = linestrip ? 1 : 2;
  for (u32 i = 1; i < num_verts; i += advance)
  {
    u32 p0 = (index + i - 1) << 2;
    u32 p1 = (index + i - 0) << 2;
    if constexpr (pr)
    {
      *index_ptr++ = p0 + 0;
      *index_ptr++ = p0 + 1;
      *index_ptr++ = p1 + 2;
      *index_ptr++ = p1 + 3;
      *index_ptr++ = s_primitive_restart;
    }
    else
    {
      *index_ptr++ = p0 + 0;
      *index_ptr++ = p0 + 1;
      *index_ptr++ = p1 + 2;
      *index_ptr++ = p0 + 1;
      *index_ptr++ = p1 + 2;
      *index_ptr++ = p1 + 3;
    }
  }
  return index_ptr;
}

u16* AddPoints(u16* index_ptr, u32 num_verts, u32 index)
{
  for (u32 i = 0; i != num_verts; ++i)
  {
    *index_ptr++ = index + i;
  }
  return index_ptr;
}

template <bool pr>
u16* AddPoints_VSExpand(u16* index_ptr, u32 num_verts, u32 index)
{
  // VS Expand uses (index >> 2) as the base vertex
  // Bottom two bits indicate which of (TL, TR, BL, BR) this is
  for (u32 i = 0; i < num_verts; ++i)
  {
    u32 base = (index + i) << 2;
    if constexpr (pr)
    {
      *index_ptr++ = base + 0;
      *index_ptr++ = base + 1;
      *index_ptr++ = base + 2;
      *index_ptr++ = base + 3;
      *index_ptr++ = s_primitive_restart;
    }
    else
    {
      *index_ptr++ = base + 0;
      *index_ptr++ = base + 1;
      *index_ptr++ = base + 2;
      *index_ptr++ = base + 1;
      *index_ptr++ = base + 2;
      *index_ptr++ = base + 3;
    }
  }
  return index_ptr;
}
}  // Anonymous namespace

void IndexGenerator::Init(bool editor_enabled)
{
  using OpcodeDecoder::Primitive;

  // When editor is enabled, we can't take shortcuts
  // as we might want to save meshes to a file
  // Some formats don't allow primitive restart
  if (g_Config.backend_info.bSupportsPrimitiveRestart && !editor_enabled)
  {
    m_primitive_table[Primitive::GX_DRAW_QUADS] = AddQuads<true>;
    m_primitive_table[Primitive::GX_DRAW_QUADS_2] = AddQuads_nonstandard<true>;
    m_primitive_table[Primitive::GX_DRAW_TRIANGLES] = AddList<true>;
    m_primitive_table[Primitive::GX_DRAW_TRIANGLE_STRIP] = AddStrip<true>;
    m_primitive_table[Primitive::GX_DRAW_TRIANGLE_FAN] = AddFan<true>;
  }
  else
  {
    m_primitive_table[Primitive::GX_DRAW_QUADS] = AddQuads<false>;
    m_primitive_table[Primitive::GX_DRAW_QUADS_2] = AddQuads_nonstandard<false>;
    m_primitive_table[Primitive::GX_DRAW_TRIANGLES] = AddList<false>;
    m_primitive_table[Primitive::GX_DRAW_TRIANGLE_STRIP] = AddStrip<false>;
    m_primitive_table[Primitive::GX_DRAW_TRIANGLE_FAN] = AddFan<false>;
  }
  if (g_Config.UseVSForLinePointExpand() && !editor_enabled)
  {
    if (g_Config.backend_info.bSupportsPrimitiveRestart)
    {
      m_primitive_table[Primitive::GX_DRAW_LINES] = AddLines_VSExpand<true, false>;
      m_primitive_table[Primitive::GX_DRAW_LINE_STRIP] = AddLines_VSExpand<true, true>;
      m_primitive_table[Primitive::GX_DRAW_POINTS] = AddPoints_VSExpand<true>;
    }
    else
    {
      m_primitive_table[Primitive::GX_DRAW_LINES] = AddLines_VSExpand<false, false>;
      m_primitive_table[Primitive::GX_DRAW_LINE_STRIP] = AddLines_VSExpand<false, true>;
      m_primitive_table[Primitive::GX_DRAW_POINTS] = AddPoints_VSExpand<false>;
    }
  }
  else
  {
    m_primitive_table[Primitive::GX_DRAW_LINES] = AddLineList;
    m_primitive_table[Primitive::GX_DRAW_LINE_STRIP] = AddLineStrip;
    m_primitive_table[Primitive::GX_DRAW_POINTS] = AddPoints;
  }
}

void IndexGenerator::Start(u16* index_ptr)
{
  m_index_buffer_current = index_ptr;
  m_base_index_ptr = index_ptr;
  m_base_index = 0;
}

void IndexGenerator::AddIndices(OpcodeDecoder::Primitive primitive, u32 num_vertices)
{
  m_index_buffer_current =
      m_primitive_table[primitive](m_index_buffer_current, num_vertices, m_base_index);
  m_base_index += num_vertices;
}

void IndexGenerator::AddExternalIndices(const u16* indices, u32 num_indices, u32 num_vertices)
{
  std::memcpy(m_index_buffer_current, indices, sizeof(u16) * num_indices);
  m_index_buffer_current += num_indices;
  m_base_index += num_vertices;
}

u32 IndexGenerator::GetRemainingIndices(OpcodeDecoder::Primitive primitive) const
{
  u32 max_index = UINT16_MAX;

  if (g_Config.UseVSForLinePointExpand() && primitive >= OpcodeDecoder::Primitive::GX_DRAW_LINES)
    max_index >>= 2;

  // Although we reserve UINT16_MAX for primitive restart, we aren't allowed to use that as an
  // actual index. But, the maximum number of vertices a game can send is UINT16_MAX, so up to
  // 0xffff indices will be used by the game. These indices would be 0x0000 through 0xfffe,
  // and since m_base_index gets incremented for each index used, after that m_base_index
  // would be 0xffff and no indices remain. If a game uses 0xfffe vertices, assuming m_base_index
  // started at 0 it would end at 0xfffe and one more index could be used. So, we do not need to
  // subtract 1 from max_index to correctly reserve one index for primitive restart.
  //
  // Pocoyo Racing uses a draw command with 0xffff vertices, which previously caused issues; see
  // https://bugs.dolphin-emu.org/issues/13136 for details.

  if (m_base_index > max_index) [[unlikely]]
  {
    PanicAlertFmt("GetRemainingIndices would overflow; we've already written too many indices? "
                  "base index {} > max index {}",
                  m_base_index, max_index);
    return 0;
  }

  return max_index - m_base_index;
}
