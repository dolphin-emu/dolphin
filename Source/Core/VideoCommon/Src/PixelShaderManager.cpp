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
static int s_nColorsChanged[2]; // 0 - regular colors, 1 - k colors
static int s_nIndTexMtxChanged;
static bool s_bAlphaChanged;
static bool s_bZBiasChanged;
static bool s_bZTextureTypeChanged;
static bool s_bDepthRangeChanged;
static bool s_bFogColorChanged;
static bool s_bFogParamChanged;
static bool s_bFogRangeAdjustChanged;
static int nLightsChanged[2]; // min,max
static float lastRGBAfull[2][4][4];
static u8 s_nTexDimsChanged;
static u8 s_nIndTexScaleChanged;
static u32 lastAlpha;
static u32 lastTexDims[8]; // width | height << 16 | wrap_s << 28 | wrap_t << 30
static u32 lastZBias;
static int nMaterialsChanged;

inline void SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	g_renderer->SetPSConstant4f(const_number, f1, f2, f3, f4);
}

inline void SetPSConstant4fv(unsigned int const_number, const float *f)
{
	g_renderer->SetPSConstant4fv(const_number, f);
}

inline void SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	g_renderer->SetMultiPSConstant4fv(const_number, count, f);
}

void PixelShaderManager::Init()
{
	lastAlpha = 0;
	memset(lastTexDims, 0, sizeof(lastTexDims));
	lastZBias = 0;
	memset(lastRGBAfull, 0, sizeof(lastRGBAfull));
	Dirty();
}

void PixelShaderManager::Dirty()
{
	s_nColorsChanged[0] = s_nColorsChanged[1] = 15;
	s_nTexDimsChanged = 0xFF;
	s_nIndTexScaleChanged = 0xFF;
	s_nIndTexMtxChanged = 15;
	s_bAlphaChanged = s_bZBiasChanged = s_bZTextureTypeChanged = s_bDepthRangeChanged = true;
	s_bFogRangeAdjustChanged = s_bFogColorChanged = s_bFogParamChanged = true;
	nLightsChanged[0] = 0; nLightsChanged[1] = 0x80;
	nMaterialsChanged = 15;
}

void PixelShaderManager::Shutdown()
{

}

void PixelShaderManager::SetConstants(u32 components)
{
	if (g_ActiveConfig.backend_info.APIType == API_OPENGL && !g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		Dirty();

	for (int i = 0; i < 2; ++i)
	{
		if (s_nColorsChanged[i])
		{
			int baseind = i ? C_KCOLORS : C_COLORS;
			for (int j = 0; j < 4; ++j)
			{
				if ((s_nColorsChanged[i] & (1 << j)))
				{
					SetPSConstant4fv(baseind+j, &lastRGBAfull[i][j][0]);
					s_nColorsChanged[i] &= ~(1<<j);
				}
			}
		}
	}

    if (s_nTexDimsChanged)
	{
		for (int i = 0; i < 8; ++i)
		{
            if ((s_nTexDimsChanged & (1<<i)))
			{
				SetPSTextureDims(i);
				s_nTexDimsChanged &= ~(1<<i);
			}
        }
    }

    if (s_bAlphaChanged)
	{
		SetPSConstant4f(C_ALPHA, (lastAlpha&0xff)/255.0f, ((lastAlpha>>8)&0xff)/255.0f, 0, ((lastAlpha>>16)&0xff)/255.0f);
		s_bAlphaChanged = false;
    }

	if (s_bZTextureTypeChanged)
	{
		float ftemp[4];
		switch (bpmem.ztex2.type)
		{
			 case 0:
				// 8 bits
				ftemp[0] = 0; ftemp[1] = 0; ftemp[2] = 0; ftemp[3] = 255.0f/16777215.0f;
				break;
			case 1:
				// 16 bits
				ftemp[0] = 255.0f/16777215.0f; ftemp[1] = 0; ftemp[2] = 0; ftemp[3] = 65280.0f/16777215.0f;
				break;
			case 2:
				// 24 bits
				ftemp[0] = 16711680.0f/16777215.0f; ftemp[1] = 65280.0f/16777215.0f; ftemp[2] = 255.0f/16777215.0f; ftemp[3] = 0;
                break;
        }
		SetPSConstant4fv(C_ZBIAS, ftemp);
		s_bZTextureTypeChanged = false;
	}

	if (s_bZBiasChanged || s_bDepthRangeChanged)
	{
		// reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
		// [0] = width/2
		// [1] = height/2
		// [2] = 16777215 * (farz - nearz)
		// [3] = xorig + width/2 + 342
		// [4] = yorig + height/2 + 342
		// [5] = 16777215 * farz

		//ERROR_LOG("pixel=%x,%x, bias=%x\n", bpmem.zcontrol.pixel_format, bpmem.ztex2.type, lastZBias);
		SetPSConstant4f(C_ZBIAS+1, xfregs.viewport.farZ / 16777216.0f, xfregs.viewport.zRange / 16777216.0f, 0, (float)(lastZBias)/16777215.0f);
		s_bZBiasChanged = s_bDepthRangeChanged = false;
	}

	// indirect incoming texture scales
	if (s_nIndTexScaleChanged)
	{
		// set as two sets of vec4s, each containing S and T of two ind stages.
		float f[8];

        if (s_nIndTexScaleChanged & 0x03)
		{
			for (u32 i = 0; i < 2; ++i)
			{
                f[2 * i] = bpmem.texscale[0].getScaleS(i & 1);
                f[2 * i + 1] = bpmem.texscale[0].getScaleT(i & 1);
                PRIM_LOG("tex indscale%d: %f %f\n", i, f[2 * i], f[2 * i + 1]);
            }
			SetPSConstant4fv(C_INDTEXSCALE, f);
        }

		if (s_nIndTexScaleChanged & 0x0c)
		{
            for (u32 i = 2; i < 4; ++i)
			{
                f[2 * i] = bpmem.texscale[1].getScaleS(i & 1);
                f[2 * i + 1] = bpmem.texscale[1].getScaleT(i & 1);
                PRIM_LOG("tex indscale%d: %f %f\n", i, f[2 * i], f[2 * i + 1]);
            }
			SetPSConstant4fv(C_INDTEXSCALE+1, &f[4]);
        }
		s_nIndTexScaleChanged = 0;
    }

	if (s_nIndTexMtxChanged)
	{
		for (int i = 0; i < 3; ++i)
		{
            if (s_nIndTexMtxChanged & (1 << i))
			{
                int scale = ((u32)bpmem.indmtx[i].col0.s0 << 0) |
					        ((u32)bpmem.indmtx[i].col1.s1 << 2) |
					        ((u32)bpmem.indmtx[i].col2.s2 << 4);
                float fscale = powf(2.0f, (float)(scale - 17)) / 1024.0f;

                // xyz - static matrix
                // TODO w - dynamic matrix scale / 256...... somehow / 4 works better
                // rev 2972 - now using / 256.... verify that this works
				SetPSConstant4f(C_INDTEXMTX + 2 * i,
					bpmem.indmtx[i].col0.ma * fscale,
					bpmem.indmtx[i].col1.mc * fscale,
					bpmem.indmtx[i].col2.me * fscale,
					fscale * 4.0f);
				SetPSConstant4f(C_INDTEXMTX + 2 * i + 1,
					bpmem.indmtx[i].col0.mb * fscale,
					bpmem.indmtx[i].col1.md * fscale,
					bpmem.indmtx[i].col2.mf * fscale,
					fscale * 4.0f);

                PRIM_LOG("indmtx%d: scale=%f, mat=(%f %f %f; %f %f %f)\n",
                	i, 1024.0f*fscale,
                	bpmem.indmtx[i].col0.ma * fscale, bpmem.indmtx[i].col1.mc * fscale, bpmem.indmtx[i].col2.me * fscale,
                	bpmem.indmtx[i].col0.mb * fscale, bpmem.indmtx[i].col1.md * fscale, bpmem.indmtx[i].col2.mf * fscale);

				s_nIndTexMtxChanged &= ~(1 << i);
			}
        }
    }

    if (s_bFogColorChanged)
	{
		SetPSConstant4f(C_FOG, bpmem.fog.color.r / 255.0f, bpmem.fog.color.g / 255.0f, bpmem.fog.color.b / 255.0f, 0);
		s_bFogColorChanged = false;
    }

    if (s_bFogParamChanged)
	{
		if(!g_ActiveConfig.bDisableFog)
		{
			//downscale magnitude to 0.24 bits
			float b = (float)bpmem.fog.b_magnitude / 0xFFFFFF;

			float b_shf = (float)(1 << bpmem.fog.b_shift);
			SetPSConstant4f(C_FOG + 1, bpmem.fog.a.GetA(), b, bpmem.fog.c_proj_fsel.GetC(), b_shf);
		}
		else
			SetPSConstant4f(C_FOG + 1, 0.0, 1.0, 0.0, 1.0);

        s_bFogParamChanged = false;
    }

	if (s_bFogRangeAdjustChanged)
	{
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
			SetPSConstant4f(C_FOG + 2, ScreenSpaceCenter, (float)Renderer::EFBToScaledX((int)(2.0f * xfregs.viewport.wd)), bpmem.fogRange.K[4].HI / 256.0f,0.0f);
		}
		else
		{
			SetPSConstant4f(C_FOG + 2, 0.0f, 1.0f, 1.0f, 0.0f); // Need to update these values for older hardware that fails to divide by zero in shaders.
		}

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
				float NormalizationCoef = 1 / 255.0f;
				SetPSConstant4f(C_PLIGHTS + 5 * i,
					((color >> 24) & 0xFF) * NormalizationCoef,
					((color >> 16) & 0xFF) * NormalizationCoef,
					((color >> 8)  & 0xFF) * NormalizationCoef,
					((color)       & 0xFF) * NormalizationCoef);
				xfmemptr += 4;

				for (int j = 0; j < 4; ++j, xfmemptr += 3)
				{
					if (j == 1 &&
						fabs(xfmemptr[0]) < 0.00001f &&
						fabs(xfmemptr[1]) < 0.00001f &&
						fabs(xfmemptr[2]) < 0.00001f)
					{
						// dist attenuation, make sure not equal to 0!!!
						SetPSConstant4f(C_PLIGHTS+5*i+j+1, 0.00001f, xfmemptr[1], xfmemptr[2], 0);
					}
					else
					{
						SetPSConstant4fv(C_PLIGHTS+5*i+j+1, xfmemptr);
					}
				}
			}

			nLightsChanged[0] = nLightsChanged[1] = -1;
		}

		if (nMaterialsChanged)
		{
			float GC_ALIGNED16(material[4]);
			float NormalizationCoef = 1 / 255.0f;

			for (int i = 0; i < 2; ++i)
			{
				if (nMaterialsChanged & (1 << i))
				{
					u32 data = *(xfregs.ambColor + i);

					material[0] = ((data >> 24) & 0xFF) * NormalizationCoef;
					material[1] = ((data >> 16) & 0xFF) * NormalizationCoef;
					material[2] = ((data >>  8) & 0xFF) * NormalizationCoef;
					material[3] = ( data        & 0xFF) * NormalizationCoef;

					SetPSConstant4fv(C_PMATERIALS + i, material);
				}
			}

			for (int i = 0; i < 2; ++i)
			{
				if (nMaterialsChanged & (1 << (i + 2)))
				{
					u32 data = *(xfregs.matColor + i);

					material[0] = ((data >> 24) & 0xFF) * NormalizationCoef;
					material[1] = ((data >> 16) & 0xFF) * NormalizationCoef;
					material[2] = ((data >>  8) & 0xFF) * NormalizationCoef;
					material[3] = ( data        & 0xFF) * NormalizationCoef;

					SetPSConstant4fv(C_PMATERIALS + i + 2, material);
				}
			}

			nMaterialsChanged = 0;
		}
	}
}

void PixelShaderManager::SetPSTextureDims(int texid)
{
	// texdims.xy are reciprocals of the real texture dimensions
	// texdims.zw are the scaled dimensions
	float fdims[4];

	TCoordInfo& tc = bpmem.texcoords[texid];
	fdims[0] = 1.0f / (float)(lastTexDims[texid] & 0xffff);
	fdims[1] = 1.0f / (float)((lastTexDims[texid] >> 16) & 0xfff);
	fdims[2] = (float)(tc.s.scale_minus_1 + 1);
	fdims[3] = (float)(tc.t.scale_minus_1 + 1);

	PRIM_LOG("texdims%d: %f %f %f %f\n", texid, fdims[0], fdims[1], fdims[2], fdims[3]);
	SetPSConstant4fv(C_TEXDIMS + texid, fdims);
}

// This one is high in profiles (0.5%).
// TODO: Move conversion out, only store the raw color value
// and update it when the shader constant is set, only.
// TODO: Conversion should be checked in the context of tev_fixes..
void PixelShaderManager::SetColorChanged(int type, int num, bool high)
{
	float *pf = &lastRGBAfull[type][num][0];

	if (!high)
	{
		int r = bpmem.tevregs[num].low.a;
		int a = bpmem.tevregs[num].low.b;
		pf[0] = (float)r * (1.0f / 255.0f);
		pf[3] = (float)a * (1.0f / 255.0f);
	}
	else
	{
		int b = bpmem.tevregs[num].high.a;
		int g = bpmem.tevregs[num].high.b;
		pf[1] = (float)g * (1.0f / 255.0f);
		pf[2] = (float)b * (1.0f / 255.0f);
	}

	s_nColorsChanged[type] |= 1 << num;
	PRIM_LOG("pixel %scolor%d: %f %f %f %f\n", type?"k":"", num, pf[0], pf[1], pf[2], pf[3]);
}

void PixelShaderManager::SetAlpha(const AlphaTest& alpha)
{
	if ((alpha.hex & 0xffff) != lastAlpha)
	{
		lastAlpha = (lastAlpha & ~0xffff) | (alpha.hex & 0xffff);
		s_bAlphaChanged = true;
	}
}

void PixelShaderManager::SetDestAlpha(const ConstantAlpha& alpha)
{
	if (alpha.alpha != (lastAlpha >> 16))
	{
		lastAlpha = (lastAlpha & ~0xff0000) | ((alpha.hex & 0xff) << 16);
		s_bAlphaChanged = true;
	}
}

void PixelShaderManager::SetTexDims(int texmapid, u32 width, u32 height, u32 wraps, u32 wrapt)
{
	u32 wh = width | (height << 16) | (wraps << 28) | (wrapt << 30);
	if (lastTexDims[texmapid] != wh)
	{
		lastTexDims[texmapid] = wh;
		s_nTexDimsChanged |= 1 << texmapid;
	}
}

void PixelShaderManager::SetZTextureBias(u32 bias)
{
	if (lastZBias != bias)
	{
		s_bZBiasChanged = true;
		lastZBias = bias;
	}
}

void PixelShaderManager::SetViewportChanged()
{
	s_bDepthRangeChanged = true;
	s_bFogRangeAdjustChanged = true; // TODO: Shouldn't be necessary with an accurate fog range adjust implementation
}

void PixelShaderManager::SetIndTexScaleChanged(u8 stagemask)
{
	s_nIndTexScaleChanged |= stagemask;
}

void PixelShaderManager::SetIndMatrixChanged(int matrixidx)
{
	s_nIndTexMtxChanged |= 1 << matrixidx;
}

void PixelShaderManager::SetZTextureTypeChanged()
{
	s_bZTextureTypeChanged = true;
}

void PixelShaderManager::SetTexCoordChanged(u8 texmapid)
{
	s_nTexDimsChanged |= 1 << texmapid;
}

void PixelShaderManager::SetFogColorChanged()
{
	s_bFogColorChanged = true;
}

void PixelShaderManager::SetFogParamChanged()
{
	s_bFogParamChanged = true;
}

void PixelShaderManager::SetFogRangeAdjustChanged()
{
	s_bFogRangeAdjustChanged = true;
}

void PixelShaderManager::SetColorMatrix(const float* pmatrix)
{
	SetMultiPSConstant4fv(C_COLORMATRIX,7,pmatrix);
	s_nColorsChanged[0] = s_nColorsChanged[1] = 15;
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

void PixelShaderManager::SetMaterialColorChanged(int index)
{
	nMaterialsChanged  |= (1 << index);
}

void PixelShaderManager::DoState(PointerWrap &p)
{
	p.Do(lastRGBAfull);
	p.Do(lastAlpha);
	p.Do(lastTexDims);
	p.Do(lastZBias);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		Dirty();
	}
}
