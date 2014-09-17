// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include "Common/CommonTypes.h"

#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

bool PixelShaderManager::s_bFogRangeAdjustChanged;
bool PixelShaderManager::s_bViewPortChanged;

std::array<int4,4> PixelShaderManager::s_tev_color;
std::array<int4,4> PixelShaderManager::s_tev_konst_color;

PixelShaderConstants PixelShaderManager::constants;
bool PixelShaderManager::dirty;

void PixelShaderManager::Init()
{
	memset(&constants, 0, sizeof(constants));
	memset(s_tev_color.data(), 0, sizeof(s_tev_color));
	memset(s_tev_konst_color.data(), 0, sizeof(s_tev_konst_color));

	Dirty();
}

void PixelShaderManager::Dirty()
{
	s_bFogRangeAdjustChanged = true;
	s_bViewPortChanged = true;

	for (unsigned index = 0; index < s_tev_color.size(); ++index)
	{
		for (int comp = 0; comp < 4; ++comp)
		{
			SetTevColor(index, comp, s_tev_color[index][comp]);
			SetTevKonstColor(index, comp, s_tev_konst_color[index][comp]);
		}
	}

	SetAlpha();
	SetDestAlpha();
	SetZTextureBias();
	SetViewportChanged();
	SetIndTexScaleChanged(false);
	SetIndTexScaleChanged(true);
	SetIndMatrixChanged(0);
	SetIndMatrixChanged(1);
	SetIndMatrixChanged(2);
	SetZTextureTypeChanged();
	SetTexCoordChanged(0);
	SetTexCoordChanged(1);
	SetTexCoordChanged(2);
	SetTexCoordChanged(3);
	SetTexCoordChanged(4);
	SetTexCoordChanged(5);
	SetTexCoordChanged(6);
	SetTexCoordChanged(7);
	SetFogColorChanged();
	SetFogParamChanged();
}

void PixelShaderManager::Shutdown()
{

}

void PixelShaderManager::SetConstants()
{
	if (s_bFogRangeAdjustChanged)
	{
		// set by two components, so keep changed flag here
		// TODO: try to split both registers and move this logic to the shader
		if (!g_ActiveConfig.bDisableFog && bpmem.fogRange.Base.Enabled == 1)
		{
			//bpmem.fogRange.Base.Center : center of the viewport in x axis. observation: bpmem.fogRange.Base.Center = realcenter + 342;
			int center = ((u32)bpmem.fogRange.Base.Center) - 342;
			// normalize center to make calculations easy
			float ScreenSpaceCenter = center / (2.0f * xfmem.viewport.wd);
			ScreenSpaceCenter = (ScreenSpaceCenter * 2.0f) - 1.0f;
			//bpmem.fogRange.K seems to be  a table of precalculated coefficients for the adjust factor
			//observations: bpmem.fogRange.K[0].LO appears to be the lowest value and bpmem.fogRange.K[4].HI the largest
			// they always seems to be larger than 256 so my theory is :
			// they are the coefficients from the center to the border of the screen
			// so to simplify I use the hi coefficient as K in the shader taking 256 as the scale
			// TODO: Shouldn't this be EFBToScaledXf?
			constants.fogf[0][0] = ScreenSpaceCenter;
			constants.fogf[0][1] = (float)Renderer::EFBToScaledX((int)(2.0f * xfmem.viewport.wd));
			constants.fogf[0][2] = bpmem.fogRange.K[4].HI / 256.0f;
		}
		else
		{
			constants.fogf[0][0] = 0;
			constants.fogf[0][1] = 1;
			constants.fogf[0][2] = 1;
		}
		dirty = true;

		s_bFogRangeAdjustChanged = false;
	}

	if (s_bViewPortChanged)
	{
		constants.zbias[1][0] = static_cast<u32>(xfmem.viewport.farZ);
		constants.zbias[1][1] = static_cast<u32>(xfmem.viewport.zRange);
		dirty = true;
		s_bViewPortChanged = false;
	}
}

void PixelShaderManager::SetTevColor(int index, int component, s32 value)
{
	auto& c = constants.colors[index];
	c[component] = s_tev_color[index][component] = value;
	dirty = true;

	PRIM_LOG("tev color%d: %d %d %d %d\n", index, c[0], c[1], c[2], c[3]);
}

void PixelShaderManager::SetTevKonstColor(int index, int component, s32 value)
{
	auto& c = constants.kcolors[index];
	c[component] = s_tev_konst_color[index][component] = value;
	dirty = true;

	PRIM_LOG("tev konst color%d: %d %d %d %d\n", index, c[0], c[1], c[2], c[3]);
}

void PixelShaderManager::SetAlpha()
{
	constants.alpha[0] = bpmem.alpha_test.ref0;
	constants.alpha[1] = bpmem.alpha_test.ref1;
	dirty = true;
}

void PixelShaderManager::SetDestAlpha()
{
	constants.alpha[3] = bpmem.dstalpha.alpha;
	dirty = true;
}

void PixelShaderManager::SetTexDims(int texmapid, u32 width, u32 height, u32 wraps, u32 wrapt)
{
	// TODO: move this check out to callee. There we could just call this function on texture changes
	// or better, use textureSize() in glsl
	if (constants.texdims[texmapid][0] != 1.0f/width || constants.texdims[texmapid][1] != 1.0f/height)
		dirty = true;

	constants.texdims[texmapid][0] = 1.0f/width;
	constants.texdims[texmapid][1] = 1.0f/height;
}

void PixelShaderManager::SetZTextureBias()
{
	constants.zbias[1][3] = bpmem.ztex1.bias;
	dirty = true;
}

void PixelShaderManager::SetViewportChanged()
{
	s_bViewPortChanged = true;
	s_bFogRangeAdjustChanged = true; // TODO: Shouldn't be necessary with an accurate fog range adjust implementation
}

void PixelShaderManager::SetIndTexScaleChanged(bool high)
{
	constants.indtexscale[high][0] = bpmem.texscale[high].ss0;
	constants.indtexscale[high][1] = bpmem.texscale[high].ts0;
	constants.indtexscale[high][2] = bpmem.texscale[high].ss1;
	constants.indtexscale[high][3] = bpmem.texscale[high].ts1;
	dirty = true;
}

void PixelShaderManager::SetIndMatrixChanged(int matrixidx)
{
	int scale = ((u32)bpmem.indmtx[matrixidx].col0.s0 << 0) |
				((u32)bpmem.indmtx[matrixidx].col1.s1 << 2) |
				((u32)bpmem.indmtx[matrixidx].col2.s2 << 4);

	// xyz - static matrix
	// w - dynamic matrix scale / 128
	constants.indtexmtx[2*matrixidx  ][0] = bpmem.indmtx[matrixidx].col0.ma;
	constants.indtexmtx[2*matrixidx  ][1] = bpmem.indmtx[matrixidx].col1.mc;
	constants.indtexmtx[2*matrixidx  ][2] = bpmem.indmtx[matrixidx].col2.me;
	constants.indtexmtx[2*matrixidx  ][3] = 17 - scale;
	constants.indtexmtx[2*matrixidx+1][0] = bpmem.indmtx[matrixidx].col0.mb;
	constants.indtexmtx[2*matrixidx+1][1] = bpmem.indmtx[matrixidx].col1.md;
	constants.indtexmtx[2*matrixidx+1][2] = bpmem.indmtx[matrixidx].col2.mf;
	constants.indtexmtx[2*matrixidx+1][3] = 17 - scale;
	dirty = true;

	PRIM_LOG("indmtx%d: scale=%d, mat=(%d %d %d; %d %d %d)\n",
			matrixidx, scale,
			bpmem.indmtx[matrixidx].col0.ma, bpmem.indmtx[matrixidx].col1.mc, bpmem.indmtx[matrixidx].col2.me,
			bpmem.indmtx[matrixidx].col0.mb, bpmem.indmtx[matrixidx].col1.md, bpmem.indmtx[matrixidx].col2.mf);

}

void PixelShaderManager::SetZTextureTypeChanged()
{
	switch (bpmem.ztex2.type)
	{
		case TEV_ZTEX_TYPE_U8:
			constants.zbias[0][0] = 0;
			constants.zbias[0][1] = 0;
			constants.zbias[0][2] = 0;
			constants.zbias[0][3] = 1;
			break;
		case TEV_ZTEX_TYPE_U16:
			constants.zbias[0][0] = 1;
			constants.zbias[0][1] = 0;
			constants.zbias[0][2] = 0;
			constants.zbias[0][3] = 256;
			break;
		case TEV_ZTEX_TYPE_U24:
			constants.zbias[0][0] = 65536;
			constants.zbias[0][1] = 256;
			constants.zbias[0][2] = 1;
			constants.zbias[0][3] = 0;
			break;
		default:
			break;
        }
        dirty = true;
}

void PixelShaderManager::SetTexCoordChanged(u8 texmapid)
{
	TCoordInfo& tc = bpmem.texcoords[texmapid];
	constants.texdims[texmapid][2] = (float)(tc.s.scale_minus_1 + 1);
	constants.texdims[texmapid][3] = (float)(tc.t.scale_minus_1 + 1);
	dirty = true;
}

void PixelShaderManager::SetFogColorChanged()
{
	if (g_ActiveConfig.bDisableFog)
		return;

	constants.fogcolor[0] = bpmem.fog.color.r;
	constants.fogcolor[1] = bpmem.fog.color.g;
	constants.fogcolor[2] = bpmem.fog.color.b;
	dirty = true;
}

void PixelShaderManager::SetFogParamChanged()
{
	if (!g_ActiveConfig.bDisableFog)
	{
		constants.fogf[1][0] = bpmem.fog.a.GetA();
		constants.fogi[1] = bpmem.fog.b_magnitude;
		constants.fogf[1][2] = bpmem.fog.c_proj_fsel.GetC();
		constants.fogi[3] = bpmem.fog.b_shift;
	}
	else
	{
		constants.fogf[1][0] = 0.f;
		constants.fogi[1] = 1;
		constants.fogf[1][2] = 0.f;
		constants.fogi[3] = 1;
	}
	dirty = true;
}

void PixelShaderManager::SetFogRangeAdjustChanged()
{
	if (g_ActiveConfig.bDisableFog)
		return;

	s_bFogRangeAdjustChanged = true;
}

void PixelShaderManager::DoState(PointerWrap &p)
{
	p.DoArray(s_tev_color);
	p.DoArray(s_tev_konst_color);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		// Reload current state from global GPU state
		// NOTE: This requires that all GPU memory has been loaded already.
		Dirty();
	}
}
