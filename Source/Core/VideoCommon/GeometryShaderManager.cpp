// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

static const int LINE_PT_TEX_OFFSETS[8] = {0, 16, 8, 4, 2, 1, 1, 1};

GeometryShaderConstants GeometryShaderManager::constants;
bool GeometryShaderManager::dirty;

static bool s_projection_changed;
static bool s_viewport_changed;

void GeometryShaderManager::Init()
{
  constants = {};

  // Init any intial constants which aren't zero when bpmem is zero.
  SetViewportChanged();
  SetProjectionChanged();

  dirty = true;
}

void GeometryShaderManager::Dirty()
{
  // This function is called after a savestate is loaded.
  // Any constants that can changed based on settings should be re-calculated
  s_projection_changed = true;

  dirty = true;
}

void GeometryShaderManager::SetConstants()
{
  if (s_projection_changed && g_ActiveConfig.stereo_mode != StereoMode::Off)
  {
    s_projection_changed = false;

    constants.stereoparams.fill(0);

    if (xfmem.projection.type == GX_PERSPECTIVE)
    {
      if (auto* const openxr_session = g_renderer->GetOpenXRSession())
      {
        Common::Matrix44 projection;
        std::memcpy(projection.data.data(), VertexShaderManager::constants.projection.data(),
                    sizeof(projection));

        // We must "undo" the game's projection before applying our eye projections.
        const auto inv_projection = projection.Inverted();

        // Use the game's near and far values.
        const float z_near = projection.data[11] / (projection.data[10] - 1);
        const float z_far = projection.data[11] / (projection.data[10] + 1);

        int eye_index = 0;
        for (auto& eye_view : constants.eye_views)
        {
          eye_view = openxr_session->GetEyeViewMatrix(eye_index, z_near, z_far) * inv_projection;
          ++eye_index;
        }
      }
      else
      {
        float offset = (g_ActiveConfig.iStereoDepth / 1000.0f) *
                       (g_ActiveConfig.iStereoDepthPercentage / 100.0f);
        constants.stereoparams[0] = g_ActiveConfig.bStereoSwapEyes ? offset : -offset;
        constants.stereoparams[1] = -constants.stereoparams[0];
        constants.stereoparams[2] = g_ActiveConfig.iStereoConvergence *
                                    (g_ActiveConfig.iStereoConvergencePercentage / 100.f);
      }
    }
    else
    {
      constants.eye_views.fill(Common::Matrix44::Identity());
    }

    dirty = true;
  }

  if (s_viewport_changed)
  {
    s_viewport_changed = false;

    constants.lineptparams[0] = 2.0f * xfmem.viewport.wd;
    constants.lineptparams[1] = -2.0f * xfmem.viewport.ht;

    dirty = true;
  }
}

void GeometryShaderManager::SetViewportChanged()
{
  s_viewport_changed = true;
}

void GeometryShaderManager::SetProjectionChanged()
{
  s_projection_changed = true;
}

void GeometryShaderManager::SetLinePtWidthChanged()
{
  constants.lineptparams[2] = bpmem.lineptwidth.linesize / 6.f;
  constants.lineptparams[3] = bpmem.lineptwidth.pointsize / 6.f;
  constants.texoffset[2] = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.lineoff];
  constants.texoffset[3] = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.pointoff];
  dirty = true;
}

void GeometryShaderManager::SetTexCoordChanged(u8 texmapid)
{
  TCoordInfo& tc = bpmem.texcoords[texmapid];
  int bitmask = 1 << texmapid;
  constants.texoffset[0] &= ~bitmask;
  constants.texoffset[0] |= tc.s.line_offset << texmapid;
  constants.texoffset[1] &= ~bitmask;
  constants.texoffset[1] |= tc.s.point_offset << texmapid;
  dirty = true;
}

void GeometryShaderManager::DoState(PointerWrap& p)
{
  p.Do(s_projection_changed);
  p.Do(s_viewport_changed);

  p.Do(constants);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    // Fixup the current state from global GPU state
    // NOTE: This requires that all GPU memory has been loaded already.
    Dirty();
  }
}
