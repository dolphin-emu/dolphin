// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

/*
Portions of this file are based off work by Markus Trenkwalder.
Copyright (c) 2007, 2008 Markus Trenkwalder

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the library's copyright owner nor the names of its
  contributors may be used to endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "VideoBackends/Software/Clipper.h"

#include "Common/Assert.h"

#include "VideoBackends/Software/NativeVertexFormat.h"
#include "VideoBackends/Software/Rasterizer.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/XFMemory.h"

namespace Clipper
{
enum
{
  NUM_CLIPPED_VERTICES = 33,
  NUM_INDICES = NUM_CLIPPED_VERTICES + 3
};

static OutputVertexData ClippedVertices[NUM_CLIPPED_VERTICES];
static OutputVertexData* Vertices[NUM_INDICES];

void Init()
{
  for (int i = 0; i < NUM_CLIPPED_VERTICES; ++i)
    Vertices[i + 3] = &ClippedVertices[i];
}

enum
{
  SKIP_FLAG = -1,
  CLIP_POS_X_BIT = 0x01,
  CLIP_NEG_X_BIT = 0x02,
  CLIP_POS_Y_BIT = 0x04,
  CLIP_NEG_Y_BIT = 0x08,
  CLIP_POS_Z_BIT = 0x10,
  CLIP_NEG_Z_BIT = 0x20
};

static inline int CalcClipMask(const OutputVertexData* v)
{
  int cmask = 0;
  const auto [x, y, z, w] = v->projectedPosition;

  if (w - x < 0)
    cmask |= CLIP_POS_X_BIT;

  if (x + w < 0)
    cmask |= CLIP_NEG_X_BIT;

  if (w - y < 0)
    cmask |= CLIP_POS_Y_BIT;

  if (y + w < 0)
    cmask |= CLIP_NEG_Y_BIT;

  if (w * z > 0)
    cmask |= CLIP_POS_Z_BIT;

  if (z + w < 0)
    cmask |= CLIP_NEG_Z_BIT;

  return cmask;
}

static inline void AddInterpolatedVertex(const float t, const int out, const int in, int* numVertices)
{
  Vertices[(*numVertices)++]->Lerp(t, Vertices[out], Vertices[in]);
}

#define DIFFERENT_SIGNS(x, y) ((x <= 0 && y > 0) || (x > 0 && y <= 0))

#define CLIP_DOTPROD(I, A, B, C, D)                                                                \
  (Vertices[I]->projectedPosition.x * A + Vertices[I]->projectedPosition.y * B +                   \
   Vertices[I]->projectedPosition.z * C + Vertices[I]->projectedPosition.w * D)

#define POLY_CLIP(PLANE_BIT, A, B, C, D)                                                           \
  {                                                                                                \
    if (mask & PLANE_BIT)                                                                          \
    {                                                                                              \
      int idxPrev = inlist[0];                                                                     \
      float dpPrev = CLIP_DOTPROD(idxPrev, A, B, C, D);                                            \
      int outcount = 0;                                                                            \
                                                                                                   \
      inlist[n] = inlist[0];                                                                       \
      for (int j = 1; j <= n; j++)                                                                 \
      {                                                                                            \
        int idx = inlist[j];                                                                       \
        float dp = CLIP_DOTPROD(idx, A, B, C, D);                                                  \
        if (dpPrev >= 0)                                                                           \
        {                                                                                          \
          outlist[outcount++] = idxPrev;                                                           \
        }                                                                                          \
                                                                                                   \
        if (DIFFERENT_SIGNS(dp, dpPrev))                                                           \
        {                                                                                          \
          if (dp < 0)                                                                              \
          {                                                                                        \
            float t = dp / (dp - dpPrev);                                                          \
            AddInterpolatedVertex(t, idx, idxPrev, &numVertices);                                  \
          }                                                                                        \
          else                                                                                     \
          {                                                                                        \
            float t = dpPrev / (dpPrev - dp);                                                      \
            AddInterpolatedVertex(t, idxPrev, idx, &numVertices);                                  \
          }                                                                                        \
          outlist[outcount++] = numVertices - 1;                                                   \
        }                                                                                          \
                                                                                                   \
        idxPrev = idx;                                                                             \
        dpPrev = dp;                                                                               \
      }                                                                                            \
                                                                                                   \
      if (outcount < 3)                                                                            \
        continue;                                                                                  \
                                                                                                   \
      {                                                                                            \
        int* tmp = inlist;                                                                         \
        inlist = outlist;                                                                          \
        outlist = tmp;                                                                             \
        n = outcount;                                                                              \
      }                                                                                            \
    }                                                                                              \
  }

#define LINE_CLIP(PLANE_BIT, A, B, C, D)                                                           \
  {                                                                                                \
    if (mask & PLANE_BIT)                                                                          \
    {                                                                                              \
      const float dp0 = CLIP_DOTPROD(0, A, B, C, D);                                               \
      const float dp1 = CLIP_DOTPROD(1, A, B, C, D);                                               \
      const bool neg_dp0 = dp0 < 0;                                                                \
      const bool neg_dp1 = dp1 < 0;                                                                \
                                                                                                   \
      if (neg_dp0 && neg_dp1)                                                                      \
        return;                                                                                    \
                                                                                                   \
      if (neg_dp1)                                                                                 \
      {                                                                                            \
        float t = dp1 / (dp1 - dp0);                                                               \
        if (t > t1)                                                                                \
          t1 = t;                                                                                  \
      }                                                                                            \
      else if (neg_dp0)                                                                            \
      {                                                                                            \
        float t = dp0 / (dp0 - dp1);                                                               \
        if (t > t0)                                                                                \
          t0 = t;                                                                                  \
      }                                                                                            \
    }                                                                                              \
  }

static void ClipTriangle(int* indices, int* numIndices)
{
  int mask = 0;

  mask |= CalcClipMask(Vertices[0]);
  mask |= CalcClipMask(Vertices[1]);
  mask |= CalcClipMask(Vertices[2]);

  if (mask != 0)
  {
    for (int i = 0; i < 3; i += 3)
    {
      int vlist[2][2 * 6 + 1];
      int *inlist = vlist[0], *outlist = vlist[1];
      int n = 3;
      int numVertices = 3;

      inlist[0] = 0;
      inlist[1] = 1;
      inlist[2] = 2;

      // mark this triangle as unused in case it should be completely
      // clipped
      indices[0] = SKIP_FLAG;
      indices[1] = SKIP_FLAG;
      indices[2] = SKIP_FLAG;

      POLY_CLIP(CLIP_POS_X_BIT, -1, 0, 0, 1);
      POLY_CLIP(CLIP_NEG_X_BIT, 1, 0, 0, 1);
      POLY_CLIP(CLIP_POS_Y_BIT, 0, -1, 0, 1);
      POLY_CLIP(CLIP_NEG_Y_BIT, 0, 1, 0, 1);
      POLY_CLIP(CLIP_POS_Z_BIT, 0, 0, 0, 1);
      POLY_CLIP(CLIP_NEG_Z_BIT, 0, 0, 1, 1);

      INCSTAT(g_stats.this_frame.num_triangles_clipped);

      // transform the poly in inlist into triangles
      indices[0] = inlist[0];
      indices[1] = inlist[1];
      indices[2] = inlist[2];
      for (int j = 3; j < n; ++j)
      {
        indices[(*numIndices)++] = inlist[0];
        indices[(*numIndices)++] = inlist[j - 1];
        indices[(*numIndices)++] = inlist[j];
      }
    }
  }
}

static void ClipLine(int* indices)
{
  int mask = 0;
  int clip_mask[2] = {0, 0};

  for (int i = 0; i < 2; ++i)
  {
    clip_mask[i] = CalcClipMask(Vertices[i]);
    mask |= clip_mask[i];
  }

  if (mask == 0)
    return;

  float t0 = 0;
  float t1 = 0;

  // Mark unused in case of early termination
  // of the macros below. (When fully clipped)
  indices[0] = SKIP_FLAG;
  indices[1] = SKIP_FLAG;

  LINE_CLIP(CLIP_POS_X_BIT, -1, 0, 0, 1);
  LINE_CLIP(CLIP_NEG_X_BIT, 1, 0, 0, 1);
  LINE_CLIP(CLIP_POS_Y_BIT, 0, -1, 0, 1);
  LINE_CLIP(CLIP_NEG_Y_BIT, 0, 1, 0, 1);
  LINE_CLIP(CLIP_POS_Z_BIT, 0, 0, -1, 1);
  LINE_CLIP(CLIP_NEG_Z_BIT, 0, 0, 1, 1);

  // Restore the old values as this line
  // was not fully clipped.
  indices[0] = 0;
  indices[1] = 1;

  int numVertices = 2;

  if (clip_mask[0])
  {
    indices[0] = numVertices;
    AddInterpolatedVertex(t0, 0, 1, &numVertices);
  }

  if (clip_mask[1])
  {
    indices[1] = numVertices;
    AddInterpolatedVertex(t1, 1, 0, &numVertices);
  }
}

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

  const bool backface = IsBackface(v0, v1, v2);

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

  int indices[NUM_INDICES] = {0,         1,         2,         SKIP_FLAG, SKIP_FLAG, SKIP_FLAG,
                              SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG,
                              SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG,
                              SKIP_FLAG, SKIP_FLAG, SKIP_FLAG};
  int numIndices = 3;

  if (backface)
  {
    Vertices[0] = v0;
    Vertices[1] = v2;
    Vertices[2] = v1;
  }
  else
  {
    Vertices[0] = v0;
    Vertices[1] = v1;
    Vertices[2] = v2;
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
    if (Vertices[0]->projectedPosition.w >= 0 && Vertices[1]->projectedPosition.w >= 0 &&
        Vertices[2]->projectedPosition.w >= 0)
      skip_clipping = true;
  }

  if (!skip_clipping)
    ClipTriangle(indices, &numIndices);

  for (int i = 0; i + 3 <= numIndices; i += 3)
  {
    ASSERT(i < NUM_INDICES);
    if (indices[i] != SKIP_FLAG)
    {
      PerspectiveDivide(Vertices[indices[i]]);
      PerspectiveDivide(Vertices[indices[i + 1]]);
      PerspectiveDivide(Vertices[indices[i + 2]]);

      Rasterizer::DrawTriangleFrontFace(Vertices[indices[i]], Vertices[indices[i + 1]],
                                        Vertices[indices[i + 2]]);
    }
  }
}

constexpr std::array<float, 8> LINE_PT_TEX_OFFSETS = {
    0, 1 / 16.f, 1 / 8.f, 1 / 4.f, 1 / 2.f, 1, 1, 1,
};

static void CopyLineVertex(OutputVertexData* dst, const OutputVertexData* src, const int px, const int py,
                           const bool apply_line_offset)
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

void ProcessLine(OutputVertexData* lineV0, OutputVertexData* lineV1)
{
  int indices[4] = {0, 1, SKIP_FLAG, SKIP_FLAG};

  Vertices[0] = lineV0;
  Vertices[1] = lineV1;

  // point to a valid vertex to store to when clipping
  Vertices[2] = &ClippedVertices[17];

  ClipLine(indices);

  if (indices[0] != SKIP_FLAG)
  {
    OutputVertexData* v0 = Vertices[indices[0]];
    OutputVertexData* v1 = Vertices[indices[1]];

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
}

static void CopyPointVertex(OutputVertexData* dst, const OutputVertexData* src, const bool px, const bool py)
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
      const auto [s, t] = bpmem.texcoords[coord_num];
      if (s.point_offset)
      {
        if (px)
          dst->texCoords[coord_num].x += (s.scale_minus_1 + 1) * point_offset;
        if (py)
          dst->texCoords[coord_num].y += (t.scale_minus_1 + 1) * point_offset;
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

bool IsTriviallyRejected(const OutputVertexData* v0, const OutputVertexData* v1,
                         const OutputVertexData* v2)
{
  int mask = CalcClipMask(v0);
  mask &= CalcClipMask(v1);
  mask &= CalcClipMask(v2);

  return mask != 0;
}

bool IsBackface(const OutputVertexData* v0, const OutputVertexData* v1, const OutputVertexData* v2)
{
  const float x0 = v0->projectedPosition.x;
  const float x1 = v1->projectedPosition.x;
  const float x2 = v2->projectedPosition.x;
  const float y1 = v1->projectedPosition.y;
  const float y0 = v0->projectedPosition.y;
  const float y2 = v2->projectedPosition.y;
  const float w0 = v0->projectedPosition.w;
  const float w1 = v1->projectedPosition.w;
  const float w2 = v2->projectedPosition.w;

  const float normalZDir = (x0 * w2 - x2 * w0) * y1 + (x2 * y0 - x0 * y2) * w1 + (y2 * w0 - y0 * w2) * x1;

  bool backface = normalZDir <= 0.0f;
  // Jimmie Johnson's Anything with an Engine has a positive viewport, while other games have a
  // negative viewport.  The positive viewport does not require vertices to be vertically mirrored,
  // but the backface test does need to be inverted for things to be drawn.
  if (xfmem.viewport.ht > 0)
    backface = !backface;

  return backface;
}

void PerspectiveDivide(OutputVertexData* vertex)
{
  const auto& [x, y, z, w] = vertex->projectedPosition;
  Vec3& screen = vertex->screenPosition;

  const float wInverse = 1.0f / w;
  screen.x = x * wInverse * xfmem.viewport.wd + xfmem.viewport.xOrig;
  screen.y = y * wInverse * xfmem.viewport.ht + xfmem.viewport.yOrig;
  screen.z = z * wInverse * xfmem.viewport.zRange + xfmem.viewport.farZ;
}
}  // namespace Clipper
