// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cfloat>
#include <cmath>

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

// track changes
static bool s_projection_changed, s_viewport_changed;

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

	dirty = true;
}

// Syncs the shader constant buffers with xfmem
void GeometryShaderManager::SetConstants()
{
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
