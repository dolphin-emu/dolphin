// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cfloat>
#include <cmath>

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

// track changes
static bool s_projection_changed, s_viewport_changed, s_lineptwidth_changed;

static const float LINE_PT_TEX_OFFSETS[8] = {
	0.f, 0.0625f, 0.125f, 0.25f, 0.5f, 1.f, 1.f, 1.f
};

GeometryShaderConstants GeometryShaderManager::constants;
bool GeometryShaderManager::dirty;

void GeometryShaderManager::Init()
{
	memset(&constants, 0, sizeof(constants));

	Dirty();
}

void GeometryShaderManager::Shutdown()
{
}

void GeometryShaderManager::Dirty()
{
	s_projection_changed = true;
	s_viewport_changed = true;
	s_lineptwidth_changed = true;

	SetTexCoordChanged(0);
	SetTexCoordChanged(1);
	SetTexCoordChanged(2);
	SetTexCoordChanged(3);
	SetTexCoordChanged(4);
	SetTexCoordChanged(5);
	SetTexCoordChanged(6);
	SetTexCoordChanged(7);

	dirty = true;
}

// Syncs the shader constant buffers with xfmem
void GeometryShaderManager::SetConstants()
{
	if (s_lineptwidth_changed)
	{
		constants.lineptwidth[0] = bpmem.lineptwidth.linesize / 6.f;
		constants.lineptwidth[1] = bpmem.lineptwidth.pointsize / 6.f;
		constants.lineptwidth[2] = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.lineoff];
		constants.lineptwidth[3] = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.pointoff];
	}

	if (s_viewport_changed)
	{
		constants.viewport[0] = 2.0f * xfmem.viewport.wd;
		constants.viewport[1] = -2.0f * xfmem.viewport.ht;
	}

	if (s_projection_changed)
	{
		s_projection_changed = false;

		if (g_ActiveConfig.iStereoMode > 0 && xfmem.projection.type == GX_PERSPECTIVE)
		{
			float offset = (g_ActiveConfig.iStereoSeparation / 1000.0f) * (g_ActiveConfig.iStereoSeparationPercent / 100.0f);
			constants.stereoparams[0] = (g_ActiveConfig.bStereoSwapEyes) ? offset : -offset;
			constants.stereoparams[1] = (g_ActiveConfig.bStereoSwapEyes) ? -offset : offset;
			constants.stereoparams[2] = (g_ActiveConfig.iStereoConvergence / 10.0f) * (g_ActiveConfig.iStereoConvergencePercent / 100.0f);
		}
		else
		{
			constants.stereoparams[0] = constants.stereoparams[1] = 0;
		}
	}

	dirty = true;
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
	s_lineptwidth_changed = true;
}

void GeometryShaderManager::SetTexCoordChanged(u8 texmapid)
{
	TCoordInfo& tc = bpmem.texcoords[texmapid];
	constants.texoffsetflags[texmapid] = tc.s.line_offset + tc.s.point_offset * 2;
	dirty = true;
}

void GeometryShaderManager::DoState(PointerWrap &p)
{
	p.Do(dirty);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		// Reload current state from global GPU state
		// NOTE: This requires that all GPU memory has been loaded already.
		Dirty();
	}
}
