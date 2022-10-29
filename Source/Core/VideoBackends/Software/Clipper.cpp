// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/Clipper.h"

#include <cmath>
#include <list>
#include <utility>

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

static constexpr float MAIN_RADIUS = 1.0f;
static constexpr float GUARDBAND_RADIUS = 2.0f;

enum class ClipPlane : u32
{
  None = 0,
  PositiveX = 1 << 0,
  NegativeX = 1 << 1,
  PositiveY = 1 << 2,
  NegativeY = 1 << 3,
  PositiveZ = 1 << 4,
  NegativeZ = 1 << 5,
};
ClipPlane operator|(ClipPlane a, ClipPlane b)
{
  return static_cast<ClipPlane>(static_cast<u32>(a) | static_cast<u32>(b));
}
ClipPlane& operator|=(ClipPlane& a, ClipPlane b)
{
  return a = a | b;
}
ClipPlane operator&(ClipPlane a, ClipPlane b)
{
  return static_cast<ClipPlane>(static_cast<u32>(a) & static_cast<u32>(b));
}
ClipPlane& operator&=(ClipPlane& a, ClipPlane b)
{
  return a = a & b;
}

struct ClipPlaneResult
{
  // Boundary coorinate values.  If the value is positive, then the point is on the inside of the
  // clip plane and thus visible.  If the value is negative, then the point is on the outside.
  float positive_x;
  float negative_x;
  float positive_y;
  float negative_y;
  float positive_z;  // far plane
  float negative_z;  // near plane
  ClipPlane planes;
};

static ClipPlaneResult CheckClipPlanes(const OutputVertexData* vertex, float radius)
{
  // The equation for a plane is (p - p_0) * n = 0, and the boundary coordinate is
  // simply (p - p_0) * n (where p is the point we're checking, p_0 is a point the plane passes
  // through, n is the normal of the plane (which we choose to point towards the origin),
  // and "*" is referring to the dot product).
  // We also know at compile-time what the values of p_0 and n are (only p varies),
  // and most entries in n are zero.
  //
  // The GameCube uses a clipping region where -1 <= x <= 1, -1 <= y <= 1, and 0 <= z <= 1 (possibly
  // < instead of <=).  However, the guardband uses -2 and 2 instead of -1 and 1.
  //
  // On the other hand, we're using homogeneous coordinates, which makes things much more
  // complicated to reason about.  One reference (Fundamentals of Computer Graphics, 4th edition,
  // section 8.1.6 Clipping against a Plane) says that this plane equation works correctly in 3D and
  // 4D; however, my attempts at creating a similar equation in 4D fail when p_0 is not (1, 0, 0, 1)
  // or similar (e.g. (2, 0, 0, 1) ends up giving different results from (1, 0, 0, 1/2) when they
  // should be the same due to homogeneity); I suspect this is due to trouble with the definition of
  // the dot product in homogeneous coordinates.
  //
  // My other reference (Jim Blinn's corner: a trip down the graphics pipeline, chapter 13: Line
  // Clipping) instead uses 0 <= x <= 1, 0 <= y <= 1, and 0 <= z <= 1, but the same process can be
  // used to derive -1 <= x <= 1 and -1 <= y <= 1.  For the guardband, rather than trying to modify
  // the plane equations, I compute p' from p by multiplying p.w by radius (which is equivalent to
  // dividing p.x, p.y, and p.z by radius) and then perform the same calculation.
  //
  // This same technique is also given in 8.1.5 Clipping in Homogenous Coordinates in Fundamentals
  // of Computer Graphics, but it doesn't explain why it works; thus, I have re-derived it below.
  ClipPlaneResult result;

  // Jim Blinn's corner: notation, notation, notation, chapter 6: calculating screen coverage gives
  // a generalization: If the left boundary is XL, and the right boundary is XR, then L = x - XL * w
  // and R = -x + XR * w.
  // Here, XL = -radius, and XR = +radius. So L = x + radius * w, and R = -x + radius * w.

  const float x = vertex->projectedPosition.x;
  const float y = vertex->projectedPosition.y;
  const float z = vertex->projectedPosition.z;
  const float w = vertex->projectedPosition.w * radius;

  // Plane X = +1, passes through (+1, 0, 0), with normal (-1, 0, 0)
  // Homogenously, these are (+1, 0, 0, 1) and (-1, 0, 0, 1)
  // We're also using p' = Vec4(p.x, p.y, p.z, p.w * radius).
  // Plane equation: (p' - Vec4(+1, 0, 0, 1)) * Vec4(-1, 0, 0, 1)
  // => Vec4(p.x - 1, p.y, p.z, p.w*radius - 1) * Vec4(-1, 0, 0, 1)
  // => (p.x - 1)*-1 + (p.w*radius - 1)
  // => (p.w * radius - p.x) + (1 - 1)
  // => p.w * radius - p.x
  // If radius is 1, that becomes simply p.w - p.x, consistent with Blinn's book.
  // For verification, let's also try p' = Vec4(p.x / radius, p.y / radius, p.z / radius,
  // p.w): Plane equation: (p' - Vec4(+1, 0, 0, 1))*Vec4(-1, 0, 0, 1)
  // => Vec4(p.x/radius - 1, p.y/radius, p.z/radius, p.w - 1) * Vec4(-1, 0, 0, 1)
  // => (p.x/radius - 1)*-1 + (p.w - 1)
  // => (p.w - p.x/radius) + (1 - 1)
  // => p.w - p.x/radius
  // Since we're solving for when the plane equation equals zero, both of these are equivalent,
  // but multiplying p.w is clearly faster than dividing three things.
  result.positive_x = w - x;

  // Plane X = -1, passes through (-1, 0, 0) with normal (+1, 0, 0)
  // Homogenously, these are (-1, 0, 0, 1) and (+1, 0, 0, 1).
  // We're still using p' = Vec4(p.x, p.y, p.z, p.w * radius).  (We could also produce the same
  // result by using the +1 plane and using -radius instead of radius, I think.)
  // Plane equation: (p' - Vec4(-1, 0, 0, 1)) * Vec4(+1, 0, 0, 1)
  // => Vec4(p.x + 1, p.y, p.z, p.w*radius - 1) * Vec4(+1, 0, 0, 1)
  // => (p.x + 1)*+1 + (p.w * radius - 1)
  // => (p.w * radius + p.x) + (1 - 1)
  // => p.w * radius + p.x
  result.negative_x = w + x;

  // Plane Y = +1, passes through (0, +1, 0) with normal (0, -1, 0).
  // Exact same derivation as X = +1.
  result.positive_y = w - y;

  // Plane Y = -1, passes through (0, -1, 0) with normal (0, +1, 0).
  // Exact same derivation as X = -1.
  result.negative_y = w + y;

  // Plane Z = +1, the far plane, passes through (0, 0, +1) with normal (0, 0, -1).
  // Exact same derivation as X = +1.
  // result.positive_z = w - z;

  // R = -z + ZR * w, ZR = 1. So R = -z + vertex->projectedPosition.w.
  // result.positive_z = -z + vertex->projectedPosition.w;
  // Say ZR = 0 instead
  result.positive_z = -z;

  // Plane Z = 0, the near plane, passes through (0, 0, 0) with normal (0, 0, +1).
  // Homogenously, these are (0, 0, 0, 1) and (0, 0, +1, 1).
  // Plane equation: (p' - Vec4(0, 0, 0, 1)) * Vec4(0, 0, +1, 1)
  // => (p.x, p.y, p.z, p.w*radius - 1) * Vec4(0, 0, +1, 1)
  // => p.z + p.w*radius - 1.
  // Hmm.  This doesn't match the result given by Blinn or Fundamental of Computer Graphics (which
  // is just z). TODO
  // result.negative_z = w + z - 1;
  // result.negative_z = w + z;

  // L = z - ZL * w. ZL = 0, so L = z.
  // result.negative_z = z;
  // Say ZL = -1 instead
  result.negative_z = z + vertex->projectedPosition.w;

  static constexpr float EPSILON = .000001F;

  result.planes = ClipPlane::None;
  if (result.positive_x <= -EPSILON)
    result.planes |= ClipPlane::PositiveX;
  if (result.negative_x <= -EPSILON)
    result.planes |= ClipPlane::NegativeX;
  if (result.positive_y <= -EPSILON)
    result.planes |= ClipPlane::PositiveY;
  if (result.negative_y <= -EPSILON)
    result.planes |= ClipPlane::NegativeY;
  if (result.positive_z <= -EPSILON)
    result.planes |= ClipPlane::PositiveZ;
  if (result.negative_z <= -EPSILON)
    result.planes |= ClipPlane::NegativeZ;

  return result;
}

static bool IsTriviallyRejected(const OutputVertexData* v0, const OutputVertexData* v1,
                                const OutputVertexData* v2);
static bool IsBackface(const OutputVertexData* v0, const OutputVertexData* v1,
                       const OutputVertexData* v2);
static void PerspectiveDivide(OutputVertexData* vertex);

static float ComputeIntersection(float bc0, float bc1)
{
  float result = bc0 / (bc0 - bc1);
  DEBUG_ASSERT(0 <= result && result <= 1);
  return result;
}

void ProcessTriangle(OutputVertexData* v0, OutputVertexData* v1, OutputVertexData* v2)
{
  INCSTAT(g_stats.this_frame.num_triangles_in);

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

  if (IsTriviallyRejected(v0, v1, v2))
  {
    if (!xfmem.clipDisable.disable_trivial_rejection)
    {
      INCSTAT(g_stats.this_frame.num_triangles_rejected);
      // NOTE: The slope used by zfreeze shouldn't be updated if the triangle is
      // trivially rejected during clipping
      return;
    }
    else
    {
      // When trivial rejection is disabled, the triangle is always drawn if it would have been
      // trivially rejected, without performing clipping on it.

      PerspectiveDivide(v0);
      PerspectiveDivide(v1);
      PerspectiveDivide(v2);

      Rasterizer::DrawTriangleFrontFace(v0, v1, v2);

      return;
    }
  }

  // Now we clip.
  ClipPlaneResult cr0 = CheckClipPlanes(v0, GUARDBAND_RADIUS);
  ClipPlaneResult cr1 = CheckClipPlanes(v1, GUARDBAND_RADIUS);
  ClipPlaneResult cr2 = CheckClipPlanes(v2, GUARDBAND_RADIUS);

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

  if (skip_clipping || (cr0.planes | cr1.planes | cr2.planes) == ClipPlane::None)
  {
    // The whole triangle is in the guardband, so we don't need to worry about clipping it.
    // I'm not sure if real hardware performs this check, but it's simple enough and should have no
    // observable effects.
    PerspectiveDivide(v0);
    PerspectiveDivide(v1);
    PerspectiveDivide(v2);

    Rasterizer::DrawTriangleFrontFace(v0, v1, v2);
    return;
  }
  else
  {
    // TODO: copies?
    std::list<std::pair<ClipPlaneResult, OutputVertexData>> vertices{
        std::make_pair(cr0, *v0),
        std::make_pair(cr1, *v1),
        std::make_pair(cr2, *v2),
    };

    static constexpr float EPSILON = .000001F;

    auto process_plane = [&vertices](ClipPlane plane, float ClipPlaneResult::*member_pointer) {
      // Assume vertices has >= 3 elements
      auto cur_itr = vertices.begin();
      auto next_itr = vertices.begin();
      next_itr++;
      auto next_next_itr = vertices.begin();
      next_next_itr++;
      next_next_itr++;
      auto prev_itr = vertices.end();
      prev_itr--;
      do
      {
        const bool prev_needs_move = (prev_itr->first.planes & plane) == plane;
        const bool cur_needs_move = (cur_itr->first.planes & plane) == plane;
        const bool next_needs_move = (next_itr->first.planes & plane) == plane;
        if (!prev_needs_move)
        {
          // Note: skip if prev_needs_move since that could be a 2-move situation.

          if (cur_needs_move)
          {
            // https://en.cppreference.com/w/cpp/language/pointer#Pointers_to_data_members
            const float prev_value = prev_itr->first.*member_pointer;
            const float cur_value = cur_itr->first.*member_pointer;
            const float next_value = next_itr->first.*member_pointer;

            // TODO: The diagrams here would be better off not showing triangles, since in practice
            // it's any 3/4 points
            if (next_needs_move)
            {
              const bool next_next_needs_move = (next_next_itr->first.planes & plane) == plane;
              if (next_next_needs_move)
              {
                // This can happen in practice:
                //
                //       O        ->       O
                //      / \       ->      / \
                //     /   \      ->     /   \
                // -------------  -> ---O-----O---
                //   /       \    ->   /       \
                //  O--     --O   ->  O---------O
                //    \--O--/
                while (next_next_itr != prev_itr && (next_next_itr->first.planes & plane) == plane)
                {
                  vertices.erase(next_itr);
                  next_itr = next_next_itr;
                  if (++next_next_itr == vertices.end())
                    next_next_itr = vertices.begin();
                }
              }

              // Move 2 vertices, in which case we don't need to add a new vertex.
              //
              //       O        ->       O
              //      / \       ->      / \
              //     /   \      ->     /   \
              // -------------  -> ---O-----O---
              //   /       \    ->
              //  O---------O   ->

              const float new_cur_value = ComputeIntersection(prev_value, cur_value);
              if (new_cur_value > EPSILON)
              {
                cur_itr->second.Lerp(new_cur_value, &prev_itr->second, &cur_itr->second);
                // TODO: Some redundant work here; we don't care about clip planes we've already
                // seen
                cur_itr->first = CheckClipPlanes(&cur_itr->second, GUARDBAND_RADIUS);
              }

              const float next_next_value = next_next_itr->first.*member_pointer;
              const float new_next_value = ComputeIntersection(next_value, next_next_value);

              if (new_next_value > EPSILON)
              {
                next_itr->second.Lerp(new_next_value, &next_itr->second, &next_next_itr->second);
                next_itr->first = CheckClipPlanes(&next_itr->second, GUARDBAND_RADIUS);
              }
            }
            else
            {
              // Move one vertex, which requires adding a new vertex.
              //
              //       O        ->
              //      / \       ->
              //     /   \      ->
              // -------------  -> ---O-----O---
              //   /       \    ->   /       \
              //  O---------O   ->  O---------O

              const float before_cur_value = ComputeIntersection(prev_value, cur_value);
              if (before_cur_value > EPSILON)
              {
                auto before_cur_itr = vertices.emplace(cur_itr);
                before_cur_itr->second.Lerp(before_cur_value, &prev_itr->second, &cur_itr->second);
                before_cur_itr->first = CheckClipPlanes(&before_cur_itr->second, GUARDBAND_RADIUS);
                prev_itr = before_cur_itr;
              }

              const float after_cur_value = ComputeIntersection(cur_value, next_value);
              if (after_cur_value > EPSILON)
              {
                cur_itr->second.Lerp(after_cur_value, &cur_itr->second, &next_itr->second);
                cur_itr->first = CheckClipPlanes(&cur_itr->second, GUARDBAND_RADIUS);
              }
            }
          }
        }

        if (++prev_itr == vertices.end())
          prev_itr = vertices.begin();
        if (++cur_itr == vertices.end())
          cur_itr = vertices.begin();
        if (++next_itr == vertices.end())
          next_itr = vertices.begin();
        if (++next_next_itr == vertices.end())
          next_next_itr = vertices.begin();
      } while (cur_itr != vertices.begin());
      // a_itr is one before the end; do the connection between that and begin()
    };

    process_plane(ClipPlane::PositiveX, &ClipPlaneResult::positive_x);
    process_plane(ClipPlane::NegativeX, &ClipPlaneResult::negative_x);
    process_plane(ClipPlane::PositiveY, &ClipPlaneResult::positive_y);
    process_plane(ClipPlane::NegativeY, &ClipPlaneResult::negative_y);
    process_plane(ClipPlane::PositiveZ, &ClipPlaneResult::positive_z);
    process_plane(ClipPlane::NegativeZ, &ClipPlaneResult::negative_z);

    auto first_vtx = vertices.begin();
    auto cur_itr = vertices.begin();
    cur_itr++;
    auto next_itr = vertices.begin();
    next_itr++;
    next_itr++;

    if (!xfmem.clipDisable.disable_copy_clipping_acceleration)
    {
      // TODO: We could save some perspective divides here maybe
      PerspectiveDivide(&first_vtx->second);
      ClipPlaneResult ncr0 = CheckClipPlanes(&first_vtx->second, MAIN_RADIUS);
      PerspectiveDivide(&cur_itr->second);
      ClipPlaneResult ncr1 = CheckClipPlanes(&cur_itr->second, MAIN_RADIUS);
      while (next_itr != vertices.end())
      {
        PerspectiveDivide(&next_itr->second);
        ClipPlaneResult ncr2 = CheckClipPlanes(&next_itr->second, MAIN_RADIUS);
        ClipPlane shared_planes = ncr0.planes & ncr1.planes & ncr2.planes;
        // Not rejected after clipping
        if (shared_planes == ClipPlane::None)
        {
          Rasterizer::DrawTriangleFrontFace(&first_vtx->second, &cur_itr->second,
                                            &next_itr->second);
        }
        cur_itr++;
        next_itr++;
        ncr1 = ncr2;
      }
    }
    else
    {
      PerspectiveDivide(&first_vtx->second);
      PerspectiveDivide(&cur_itr->second);
      while (next_itr != vertices.end())
      {
        PerspectiveDivide(&next_itr->second);
        Rasterizer::DrawTriangleFrontFace(&first_vtx->second, &cur_itr->second, &next_itr->second);
        cur_itr++;
        next_itr++;
      }
    }
  }
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
  // TODO: How does clipping for lines work? For now, I'm just using the main radius (no guardband).
  ClipPlaneResult cr0 = CheckClipPlanes(v0, MAIN_RADIUS);
  ClipPlaneResult cr1 = CheckClipPlanes(v1, MAIN_RADIUS);

  if ((cr0.planes & cr1.planes) != ClipPlane::None)
  {
    // Trivially rejected
    return;
  }

  OutputVertexData nv0 = *v0;
  OutputVertexData nv1 = *v1;
  if ((cr0.planes | cr1.planes) != ClipPlane::None)
  {
    // Clipping is needed
    float a0 = 0;
    float a1 = 1;

    auto process_plane = [&](ClipPlane plane, float ClipPlaneResult::*member_pointer) {
      if ((cr0.planes & plane) == plane)
        a0 = std::max(a0, ComputeIntersection(cr0.*member_pointer, cr1.*member_pointer));
      else if ((cr1.planes & plane) == plane)
        a1 = std::min(a1, ComputeIntersection(cr0.*member_pointer, cr1.*member_pointer));
    };

    process_plane(ClipPlane::PositiveX, &ClipPlaneResult::positive_x);
    process_plane(ClipPlane::NegativeX, &ClipPlaneResult::negative_x);
    process_plane(ClipPlane::PositiveY, &ClipPlaneResult::positive_y);
    process_plane(ClipPlane::NegativeY, &ClipPlaneResult::negative_y);
    process_plane(ClipPlane::PositiveZ, &ClipPlaneResult::positive_z);
    process_plane(ClipPlane::NegativeZ, &ClipPlaneResult::negative_z);

    if (a0 >= a1)
    {
      // Nontrivial rejection
      return;
    }

    if (a0 != 0)
      nv0.Lerp(a0, v0, v1);
    if (a1 != 1)
      nv1.Lerp(a1, v0, v1);

    v0 = &nv0;
    v1 = &nv1;
  }

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
  ClipPlaneResult r0 = CheckClipPlanes(v0, MAIN_RADIUS);
  ClipPlaneResult r1 = CheckClipPlanes(v1, MAIN_RADIUS);
  ClipPlaneResult r2 = CheckClipPlanes(v2, MAIN_RADIUS);
  ClipPlane shared_planes = r0.planes & r1.planes & r2.planes;
  // If all 3 points are on the wrong side of the same clip plane, then the triangle is trivially
  // off screen.
  return shared_planes != ClipPlane::None;
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
  // Left bound definitely seems to be 0 (I get near-perfect matches)
  // Right bound doesn't seem to be either 1024, 2048, or their average (1536). Needs more checking.
  // This only is a thing in practice when clipping is disabled.
  // screen.x = std::clamp(screen.x, 0.f, 2048.f);
  // screen.y = std::clamp(screen.y, 0.f, 2048.f);
}
}  // namespace Clipper
