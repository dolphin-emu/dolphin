// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include "Common.h"
#include "Statistics.h"
#include "PixelShaderManager.h"
#include "VideoCommon.h"
#include "VideoConfig.h"

#include "RenderBase.h"
static bool s_bFogRangeAdjustChanged;
static bool s_bViewPortChanged;
static int nLightsChanged[2]; // min,max

PixelShaderConstants PixelShaderManager::constants;
bool PixelShaderManager::dirty;

void PixelShaderManager::Init()
{
	memset(&constants, 0, sizeof(constants));
	Dirty();
}

void PixelShaderManager::Dirty()
{
	s_bFogRangeAdjustChanged = true;
	s_bViewPortChanged = true;
	nLightsChanged[0] = 0; nLightsChanged[1] = 0x80;

	SetColorChanged(0, 0);
	SetColorChanged(0, 1);
	SetColorChanged(0, 2);
	SetColorChanged(0, 3);
	SetColorChanged(1, 0);
	SetColorChanged(1, 1);
	SetColorChanged(1, 2);
	SetColorChanged(1, 3);
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
		if(!g_ActiveConfig.bDisableFog && bpmem.fogRange.Base.Enabled == 1)
		{
			//bpmem.fogRange.Base.Center : center of the viewport in x axis. observation: bpmem.fogRange.Base.Center = realcenter + 342;
			int center = ((u32)bpmem.fogRange.Base.Center) - 342;
			// normalize center to make calculations easy
			float ScreenSpaceCenter = center / (2.0f * xfregs.viewport.wd);
			ScreenSpaceCenter = (ScreenSpaceCenter * 2.0f) - 1.0f;
			//bpmem.fogRange.K seems to be  a table of precalculated coefficients for the adjust factor
			//observations: bpmem.fogRange.K[0].LO appears to be the lowest value and bpmem.fogRange.K[4].HI the largest
			// they always seems to be larger than 256 so my theory is :
			// they are the coefficients from the center to the border of the screen
			// so to simplify I use the hi coefficient as K in the shader taking 256 as the scale
			constants.fog[2][0] = ScreenSpaceCenter;
			constants.fog[2][1] = (float)Renderer::EFBToScaledX((int)(2.0f * xfregs.viewport.wd));
			constants.fog[2][2] = bpmem.fogRange.K[4].HI / 256.0f;
		}
		else
		{
			constants.fog[2][0] = 0;
			constants.fog[2][1] = 1;
			constants.fog[2][2] = 1;
		}
		dirty = true;

		s_bFogRangeAdjustChanged = false;
	}

	if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)  // config check added because the code in here was crashing for me inside SetPSConstant4f
	{
		if (nLightsChanged[0] >= 0)
		{
			// lights don't have a 1 to 1 mapping, the color component needs to be converted to 4 floats
			int istart = nLightsChanged[0] / 0x10;
			int iend = (nLightsChanged[1] + 15) / 0x10;
			const float* xfmemptr = (const float*)&xfmem[0x10 * istart + XFMEM_LIGHTS];

			for (int i = istart; i < iend; ++i)
			{
				u32 color = *(const u32*)(xfmemptr + 3);
				constants.plights[5*i][0] = ((color >> 24) & 0xFF) / 255.0f;
				constants.plights[5*i][1] = ((color >> 16) & 0xFF) / 255.0f;
				constants.plights[5*i][2] = ((color >> 8)  & 0xFF) / 255.0f;
				constants.plights[5*i][3] = ((color)       & 0xFF) / 255.0f;
				xfmemptr += 4;

				for (int j = 0; j < 4; ++j, xfmemptr += 3)
				{
					if (j == 1 &&
						fabs(xfmemptr[0]) < 0.00001f &&
						fabs(xfmemptr[1]) < 0.00001f &&
						fabs(xfmemptr[2]) < 0.00001f)
						// dist attenuation, make sure not equal to 0!!!
						constants.plights[5*i+j+1][0] = 0.00001f;
					else
						constants.plights[5*i+j+1][0] = xfmemptr[0];
					constants.plights[5*i+j+1][1] = xfmemptr[1];
					constants.plights[5*i+j+1][2] = xfmemptr[2];
				}
			}
			dirty = true;

			nLightsChanged[0] = nLightsChanged[1] = -1;
		}
	}

	if(s_bViewPortChanged)
	{
		constants.zbias[1][0] = xfregs.viewport.farZ / 16777216.0f;
		constants.zbias[1][1] = xfregs.viewport.zRange / 16777216.0f;
		dirty = true;
		s_bViewPortChanged = false;
	}
}

// This one is high in profiles (0.5%).
// TODO: Move conversion out, only store the raw color value
// and update it when the shader constant is set, only.
// TODO: Conversion should be checked in the context of tev_fixes..
void PixelShaderManager::SetColorChanged(int type, int num)
{
	float4* c = type ? constants.kcolors : constants.colors;
	c[num][0] = bpmem.tevregs[num].low.a / 255.0f;
	c[num][3] = bpmem.tevregs[num].low.b / 255.0f;
	c[num][2] = bpmem.tevregs[num].high.a / 255.0f;
	c[num][1] = bpmem.tevregs[num].high.b / 255.0f;
	dirty = true;

	PRIM_LOG("pixel %scolor%d: %f %f %f %f\n", type?"k":"", num, c[num][0], c[num][1], c[num][2], c[num][3]);
}

void PixelShaderManager::SetAlpha()
{
	constants.alpha[0] = bpmem.alpha_test.ref0 / 255.0f;
	constants.alpha[1] = bpmem.alpha_test.ref1 / 255.0f;
	dirty = true;
}

void PixelShaderManager::SetDestAlpha()
{
	constants.alpha[3] = bpmem.dstalpha.alpha / 255.0f;
	dirty = true;
}

void PixelShaderManager::SetTexDims(int texmapid, u32 width, u32 height, u32 wraps, u32 wrapt)
{
	// TODO: move this check out to callee. There we could just call this function on texture changes
	// or better, use textureSize() in glsl
	if(constants.texdims[texmapid][0] != 1.0f/width || constants.texdims[texmapid][1] != 1.0f/height)
		dirty = true;

	constants.texdims[texmapid][0] = 1.0f/width;
	constants.texdims[texmapid][1] = 1.0f/height;
}

void PixelShaderManager::SetZTextureBias()
{
	constants.zbias[1][3] = bpmem.ztex1.bias/16777215.0f;
	dirty = true;
}

void PixelShaderManager::SetViewportChanged()
{
	s_bViewPortChanged = true;
	s_bFogRangeAdjustChanged = true; // TODO: Shouldn't be necessary with an accurate fog range adjust implementation
}

void PixelShaderManager::SetIndTexScaleChanged(bool high)
{
	constants.indtexscale[high][0] = bpmem.texscale[high].getScaleS(0);
	constants.indtexscale[high][1] = bpmem.texscale[high].getScaleT(0);
	constants.indtexscale[high][2] = bpmem.texscale[high].getScaleS(1);
	constants.indtexscale[high][3] = bpmem.texscale[high].getScaleT(1);
	dirty = true;
}

void PixelShaderManager::SetIndMatrixChanged(int matrixidx)
{
	int scale = ((u32)bpmem.indmtx[matrixidx].col0.s0 << 0) |
			((u32)bpmem.indmtx[matrixidx].col1.s1 << 2) |
			((u32)bpmem.indmtx[matrixidx].col2.s2 << 4);
	float fscale = powf(2.0f, (float)(scale - 17)) / 1024.0f;

	// xyz - static matrix
	// TODO w - dynamic matrix scale / 256...... somehow / 4 works better
	// rev 2972 - now using / 256.... verify that this works
	constants.indtexmtx[2*matrixidx][0] = bpmem.indmtx[matrixidx].col0.ma * fscale;
	constants.indtexmtx[2*matrixidx][1] = bpmem.indmtx[matrixidx].col1.mc * fscale;
	constants.indtexmtx[2*matrixidx][2] = bpmem.indmtx[matrixidx].col2.me * fscale;
	constants.indtexmtx[2*matrixidx][3] = fscale * 4.0f;
	constants.indtexmtx[2*matrixidx+1][0] = bpmem.indmtx[matrixidx].col0.mb * fscale;
	constants.indtexmtx[2*matrixidx+1][1] = bpmem.indmtx[matrixidx].col1.md * fscale;
	constants.indtexmtx[2*matrixidx+1][2] = bpmem.indmtx[matrixidx].col2.mf * fscale;
	constants.indtexmtx[2*matrixidx+1][3] = fscale * 4.0f;
	dirty = true;

	PRIM_LOG("indmtx%d: scale=%f, mat=(%f %f %f; %f %f %f)\n",
			matrixidx, 1024.0f*fscale,
			bpmem.indmtx[matrixidx].col0.ma * fscale, bpmem.indmtx[matrixidx].col1.mc * fscale, bpmem.indmtx[matrixidx].col2.me * fscale,
		bpmem.indmtx[matrixidx].col0.mb * fscale, bpmem.indmtx[matrixidx].col1.md * fscale, bpmem.indmtx[matrixidx].col2.mf * fscale);

}

void PixelShaderManager::SetZTextureTypeChanged()
{
	switch (bpmem.ztex2.type)
	{
		case TEV_ZTEX_TYPE_U8:
			constants.zbias[0][0] = 0;
			constants.zbias[0][1] = 0;
			constants.zbias[0][2] = 0;
			constants.zbias[0][3] = 255.0f/16777215.0f;
			break;
		case TEV_ZTEX_TYPE_U16:
			constants.zbias[0][0] = 255.0f/16777215.0f;
			constants.zbias[0][1] = 0;
			constants.zbias[0][2] = 0;
			constants.zbias[0][3] = 65280.0f/16777215.0f;
			break;
		case TEV_ZTEX_TYPE_U24:
			constants.zbias[0][0] = 16711680.0f/16777215.0f;
			constants.zbias[0][1] = 65280.0f/16777215.0f;
			constants.zbias[0][2] = 255.0f/16777215.0f;
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
	constants.fog[0][0] = bpmem.fog.color.r / 255.0f;
	constants.fog[0][1] = bpmem.fog.color.g / 255.0f;
	constants.fog[0][2] = bpmem.fog.color.b / 255.0f;
	dirty = true;
}

void PixelShaderManager::SetFogParamChanged()
{
	if(!g_ActiveConfig.bDisableFog)
	{
		constants.fog[1][0] = bpmem.fog.a.GetA();
		constants.fog[1][1] = (float)bpmem.fog.b_magnitude / 0xFFFFFF;
		constants.fog[1][2] = bpmem.fog.c_proj_fsel.GetC();
		constants.fog[1][3] = (float)(1 << bpmem.fog.b_shift);
	}
	else
	{
		constants.fog[1][0] = 0;
		constants.fog[1][1] = 1;
		constants.fog[1][2] = 0;
		constants.fog[1][3] = 1;
	}
	dirty = true;
}

void PixelShaderManager::SetFogRangeAdjustChanged()
{
	s_bFogRangeAdjustChanged = true;
}

void PixelShaderManager::InvalidateXFRange(int start, int end)
{
	if (start < XFMEM_LIGHTS_END && end > XFMEM_LIGHTS)
	{
		int _start = start < XFMEM_LIGHTS ? XFMEM_LIGHTS : start-XFMEM_LIGHTS;
		int _end = end < XFMEM_LIGHTS_END ? end-XFMEM_LIGHTS : XFMEM_LIGHTS_END-XFMEM_LIGHTS;

		if (nLightsChanged[0] == -1 )
		{
			nLightsChanged[0] = _start;
			nLightsChanged[1] = _end;
		}
		else
		{
			if (nLightsChanged[0] > _start) nLightsChanged[0] = _start;
			if (nLightsChanged[1] < _end)   nLightsChanged[1] = _end;
		}
	}
}

void PixelShaderManager::SetMaterialColorChanged(int index, u32 color)
{
	if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
	{
		constants.pmaterials[index][0] = ((color >> 24) & 0xFF) / 255.0f;
		constants.pmaterials[index][1] = ((color >> 16) & 0xFF) / 255.0f;
		constants.pmaterials[index][2] = ((color >>  8) & 0xFF) / 255.0f;
		constants.pmaterials[index][3] = ( color        & 0xFF) / 255.0f;
		dirty = true;
	}
}

void PixelShaderManager::DoState(PointerWrap &p)
{
	p.Do(constants);
	p.Do(dirty);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		Dirty();
	}
}
