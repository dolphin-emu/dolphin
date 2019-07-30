// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  Vec4 pos = v->projectedPosition;

  if (pos.w - pos.x < 0)
    cmask |= CLIP_POS_X_BIT;

  if (pos.x + pos.w < 0)
    cmask |= CLIP_NEG_X_BIT;

  if (pos.w - pos.y < 0)
    cmask |= CLIP_POS_Y_BIT;

  if (pos.y + pos.w < 0)
    cmask |= CLIP_NEG_Y_BIT;

  if (pos.w * pos.z > 0)
    cmask |= CLIP_POS_Z_BIT;

  if (pos.z + pos.w < 0)
    cmask |= CLIP_NEG_Z_BIT;

  return cmask;
}

static inline void AddInterpolatedVertex(float t, int out, int in, int* numVertices)
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
  INCSTAT(g_stats.this_frame.num_triangles_in)

  bool backface;

  if (!CullTest(v0, v1, v2, backface))
    return;

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

static void CopyVertex(OutputVertexData* dst, const OutputVertexData* src, float dx, float dy,
                       unsigned int sOffset)
{
  dst->screenPosition.x = src->screenPosition.x + dx;
  dst->screenPosition.y = src->screenPosition.y + dy;
  dst->screenPosition.z = src->screenPosition.z;

  dst->normal = src->normal;
  dst->color = src->color;

  // todo - s offset
  dst->texCoords = src->texCoords;
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

    float dx = v1->screenPosition.x - v0->screenPosition.x;
    float dy = v1->screenPosition.y - v0->screenPosition.y;

    float screenDx = 0;
    float screenDy = 0;

    if (fabsf(dx) > fabsf(dy))
    {
      if (dx > 0)
        screenDy = bpmem.lineptwidth.linesize / -12.0f;
      else
        screenDy = bpmem.lineptwidth.linesize / 12.0f;
    }
    else
    {
      if (dy > 0)
        screenDx = bpmem.lineptwidth.linesize / 12.0f;
      else
        screenDx = bpmem.lineptwidth.linesize / -12.0f;
    }

    OutputVertexData triangle[3];

    CopyVertex(&triangle[0], v0, screenDx, screenDy, 0);
    CopyVertex(&triangle[1], v1, screenDx, screenDy, 0);
    CopyVertex(&triangle[2], v1, -screenDx, -screenDy, bpmem.lineptwidth.lineoff);

    // ccw winding
    Rasterizer::DrawTriangleFrontFace(&triangle[2], &triangle[1], &triangle[0]);

    CopyVertex(&triangle[1], v0, -screenDx, -screenDy, bpmem.lineptwidth.lineoff);

    Rasterizer::DrawTriangleFrontFace(&triangle[0], &triangle[1], &triangle[2]);
  }
}

bool CullTest(const OutputVertexData* v0, const OutputVertexData* v1, const OutputVertexData* v2,
              bool& backface)
{
  int mask = CalcClipMask(v0);
  mask &= CalcClipMask(v1);
  mask &= CalcClipMask(v2);

  if (mask)
  {
    INCSTAT(g_stats.this_frame.num_triangles_rejected)
    return false;
  }

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

  backface = normalZDir <= 0.0f;

  if ((bpmem.genMode.cullmode & 1) && !backface)  // cull frontfacing
  {
    INCSTAT(g_stats.this_frame.num_triangles_culled)
    return false;
  }

  if ((bpmem.genMode.cullmode & 2) && backface)  // cull backfacing
  {
    INCSTAT(g_stats.this_frame.num_triangles_culled)
    return false;
  }

  return true;
}

void PerspectiveDivide(OutputVertexData* vertex)
{
  Vec4& projected = vertex->projectedPosition;
  Vec3& screen = vertex->screenPosition;

  float wInverse = 1.0f / projected.w;
  screen.x = projected.x * wInverse * xfmem.viewport.wd + xfmem.viewport.xOrig - 342;
  screen.y = projected.y * wInverse * xfmem.viewport.ht + xfmem.viewport.yOrig - 342;
  screen.z = projected.z * wInverse * xfmem.viewport.zRange + xfmem.viewport.farZ;
}
}  // namespace Clipper
