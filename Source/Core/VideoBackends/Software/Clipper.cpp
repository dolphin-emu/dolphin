// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/Clipper.h"

#include "Common/Assert.h"

#include "VideoBackends/Software/NativeVertexFormat.h"
#include "VideoBackends/Software/Rasterizer.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/XFMemory.h"

namespace Clipper
{
void Init()
{
}

static bool IsTriviallyRejected(const OutputVertexData* v0, const OutputVertexData* v1,
                                const OutputVertexData* v2);
static bool IsBackface(const OutputVertexData* v0, const OutputVertexData* v1,
                       const OutputVertexData* v2);
static void PerspectiveDivide(OutputVertexData* vertex);

void ProcessTriangle(OutputVertexData* v0, OutputVertexData* v1, OutputVertexData* v2)
{
  INCSTAT(g_stats.this_frame.num_triangles_in);

  if (IsTriviallyRejected(v0, v1, v2))
  {
    INCSTAT(g_stats.this_frame.num_triangles_rejected);
    // NOTE: The slope used by zfreeze shouldn't be updated if the triangle is
    // trivially rejected during clipping
    return;
  }

  bool backface = IsBackface(v0, v1, v2);

  if (!backface)
  {
    if (bpmem.genMode.cullmode == CullMode::Back || bpmem.genMode.cullmode == CullMode::All)
    {
      // cull frontfacing - we still need to update the slope for zfreeze
      PerspectiveDivide(v0);
      PerspectiveDivide(v1);
      PerspectiveDivide(v2);
      Rasterizer::UpdateZSlope(v0, v1, v2, bpmem.scissorOffset.x * 2, bpmem.scissorOffset.y * 2);
      INCSTAT(g_stats.this_frame.num_triangles_culled);
      return;
    }
  }
  else
  {
    if (bpmem.genMode.cullmode == CullMode::Front || bpmem.genMode.cullmode == CullMode::All)
    {
      // cull backfacing - we still need to update the slope for zfreeze
      PerspectiveDivide(v0);
      PerspectiveDivide(v2);
      PerspectiveDivide(v1);
      Rasterizer::UpdateZSlope(v0, v2, v1, bpmem.scissorOffset.x * 2, bpmem.scissorOffset.y * 2);
      INCSTAT(g_stats.this_frame.num_triangles_culled);
      return;
    }
  }

  if (backface)
  {
    std::swap(v1, v2);
  }

  // TODO: behavior when disable_clipping_detection is set doesn't quite match actual hardware;
  // there does still seem to be a maximum size after which things are clipped.  Also, currently
  // when clipping is enabled triangles are clipped to exactly the viewport, but on hardware there
  // is a guardband (and with certain scissor configurations, things can show up in it)
  // Mario Party 8 in widescreen breaks without this: https://bugs.dolphin-emu.org/issues/12769
  bool skip_clipping = false;
  if (xfmem.clipDisable.disable_clipping_detection)
  {
    // If any w coordinate is negative, then the perspective divide will flip coordinates, breaking
    // various assumptions (including backface).  So, we still need to do clipping in that case.
    // This isn't the actual condition hardware uses.
    if (v0->projectedPosition.w >= 0 && v1->projectedPosition.w >= 0 &&
        v2->projectedPosition.w >= 0)
    {
      skip_clipping = true;
    }
  }

  if (skip_clipping)
  {
    // TODO
  }

  PerspectiveDivide(v0);
  PerspectiveDivide(v1);
  PerspectiveDivide(v2);

  Rasterizer::DrawTriangleFrontFace(v0, v1, v2);
}

constexpr std::array<float, 8> LINE_PT_TEX_OFFSETS = {
    0, 1 / 16.f, 1 / 8.f, 1 / 4.f, 1 / 2.f, 1, 1, 1,
};

static void CopyLineVertex(OutputVertexData* dst, const OutputVertexData* src, int px, int py,
                           bool apply_line_offset)
{
  const float line_half_width = bpmem.lineptwidth.linesize / 12.0f;

  dst->projectedPosition = src->projectedPosition;
  dst->screenPosition.x = src->screenPosition.x + px * line_half_width;
  dst->screenPosition.y = src->screenPosition.y + py * line_half_width;
  dst->screenPosition.z = src->screenPosition.z;

  dst->normal = src->normal;
  dst->color = src->color;

  dst->texCoords = src->texCoords;

  if (apply_line_offset && LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.lineoff] != 0)
  {
    for (u32 coord_num = 0; coord_num < xfmem.numTexGen.numTexGens; coord_num++)
    {
      if (bpmem.texcoords[coord_num].s.line_offset)
      {
        dst->texCoords[coord_num].x += (bpmem.texcoords[coord_num].s.scale_minus_1 + 1) *
                                       LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.lineoff];
      }
    }
  }
}

void ProcessLine(OutputVertexData* v0, OutputVertexData* v1)
{
  PerspectiveDivide(v0);
  PerspectiveDivide(v1);

  const float dx = v1->screenPosition.x - v0->screenPosition.x;
  const float dy = v1->screenPosition.y - v0->screenPosition.y;

  int px = 0;
  int py = 0;

  // GameCube/Wii's line drawing algorithm is a little quirky. It does not
  // use the correct line caps. Instead, the line caps are vertical or
  // horizontal depending the slope of the line.
  // FIXME: What does real hardware do when line is at a 45-degree angle?

  // Note that py or px are set positive or negative to ensure that the triangles are drawn ccw.
  if (fabsf(dx) > fabsf(dy))
    py = (dx > 0) ? -1 : 1;
  else
    px = (dy > 0) ? 1 : -1;

  OutputVertexData triangle[3];

  CopyLineVertex(&triangle[0], v0, px, py, false);
  CopyLineVertex(&triangle[1], v1, px, py, false);
  CopyLineVertex(&triangle[2], v1, -px, -py, true);

  // ccw winding
  Rasterizer::DrawTriangleFrontFace(&triangle[2], &triangle[1], &triangle[0]);

  CopyLineVertex(&triangle[1], v0, -px, -py, true);

  Rasterizer::DrawTriangleFrontFace(&triangle[0], &triangle[1], &triangle[2]);
}

static void CopyPointVertex(OutputVertexData* dst, const OutputVertexData* src, bool px, bool py)
{
  const float point_radius = bpmem.lineptwidth.pointsize / 12.0f;

  dst->projectedPosition = src->projectedPosition;
  dst->screenPosition.x = src->screenPosition.x + (px ? 1 : -1) * point_radius;
  dst->screenPosition.y = src->screenPosition.y + (py ? 1 : -1) * point_radius;
  dst->screenPosition.z = src->screenPosition.z;

  dst->normal = src->normal;
  dst->color = src->color;

  dst->texCoords = src->texCoords;

  const float point_offset = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.pointoff];
  if (point_offset != 0)
  {
    for (u32 coord_num = 0; coord_num < xfmem.numTexGen.numTexGens; coord_num++)
    {
      const auto coord_info = bpmem.texcoords[coord_num];
      if (coord_info.s.point_offset)
      {
        if (px)
          dst->texCoords[coord_num].x += (coord_info.s.scale_minus_1 + 1) * point_offset;
        if (py)
          dst->texCoords[coord_num].y += (coord_info.t.scale_minus_1 + 1) * point_offset;
      }
    }
  }
}

void ProcessPoint(OutputVertexData* center)
{
  // TODO: This isn't actually doing any clipping
  PerspectiveDivide(center);

  OutputVertexData ll, lr, ul, ur;

  CopyPointVertex(&ll, center, false, false);
  CopyPointVertex(&lr, center, true, false);
  CopyPointVertex(&ur, center, true, true);
  CopyPointVertex(&ul, center, false, true);

  Rasterizer::DrawTriangleFrontFace(&ll, &ul, &lr);
  Rasterizer::DrawTriangleFrontFace(&ur, &lr, &ul);
}

static bool IsTriviallyRejected(const OutputVertexData* v0, const OutputVertexData* v1,
                                const OutputVertexData* v2)
{
  // TODO
  return false;
}

static bool IsBackface(const OutputVertexData* v0, const OutputVertexData* v1,
                       const OutputVertexData* v2)
{
  float x0 = v0->projectedPosition.x;
  float x1 = v1->projectedPosition.x;
  float x2 = v2->projectedPosition.x;
  float y1 = v1->projectedPosition.y;
  float y0 = v0->projectedPosition.y;
  float y2 = v2->projectedPosition.y;
  float w0 = v0->projectedPosition.w;
  float w1 = v1->projectedPosition.w;
  float w2 = v2->projectedPosition.w;

  float normalZDir = (x0 * w2 - x2 * w0) * y1 + (x2 * y0 - x0 * y2) * w1 + (y2 * w0 - y0 * w2) * x1;

  bool backface = normalZDir <= 0.0f;
  // Jimmie Johnson's Anything with an Engine has a positive viewport, while other games have a
  // negative viewport.  The positive viewport does not require vertices to be vertically mirrored,
  // but the backface test does need to be inverted for things to be drawn.
  if (xfmem.viewport.ht > 0)
    backface = !backface;

  return backface;
}

static void PerspectiveDivide(OutputVertexData* vertex)
{
  Vec4& projected = vertex->projectedPosition;
  Vec3& screen = vertex->screenPosition;

  float wInverse = 1.0f / projected.w;
  screen.x = projected.x * wInverse * xfmem.viewport.wd + xfmem.viewport.xOrig;
  screen.y = projected.y * wInverse * xfmem.viewport.ht + xfmem.viewport.yOrig;
  screen.z = projected.z * wInverse * xfmem.viewport.zRange + xfmem.viewport.farZ;
}
}  // namespace Clipper
