// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <cmath>

#include "Common.h"
#include "Profiler.h"
#include "Statistics.h"
#include "PixelShaderManager.h"
#include "VideoCommon.h"

static int s_nColorsChanged[2]; // 0 - regular colors, 1 - k colors
static int s_nIndTexMtxChanged = 0;
static bool s_bAlphaChanged;
static bool s_bZBiasChanged;
static bool s_bZTextureTypeChanged;
static bool s_bDepthRangeChanged;
static bool s_bFogColorChanged;
static bool s_bFogParamChanged;
static float lastDepthRange[2] = {0}; // 0 = far z, 1 = far - near
static float lastRGBAfull[2][4][4];
static float lastCustomTexScale[8][2];
static u8 s_nTexDimsChanged;
static u8 s_nIndTexScaleChanged;
static u32 lastAlpha = 0;
static u32 lastTexDims[8]={0}; // width | height << 16 | wrap_s << 28 | wrap_t << 30
static u32 lastZBias = 0;

// lower byte describes if a texture is nonpow2 or pow2
// next byte describes whether the repeat wrap mode is enabled for the s channel
// next byte is for t channel
static u32 s_texturemask = 0;

void PixelShaderManager::Init()
{
    s_nColorsChanged[0] = s_nColorsChanged[1] = 0;
    s_nTexDimsChanged = 0;
    s_nIndTexScaleChanged = 0;
    s_nIndTexMtxChanged = 15;
    s_bAlphaChanged = s_bZBiasChanged = s_bZTextureTypeChanged = s_bDepthRangeChanged = true;
    s_bFogColorChanged = s_bFogParamChanged = true;
    memset(lastRGBAfull, 0, sizeof(lastRGBAfull));
    for (int i = 0; i < 8; i++)
		lastCustomTexScale[i][0] = lastCustomTexScale[i][1] = 1.0f;
}

void PixelShaderManager::Shutdown()
{

}

void PixelShaderManager::SetConstants()
{
    for (int i = 0; i < 2; ++i) 
	{
        if (s_nColorsChanged[i]) 
		{
            int baseind = i ? C_KCOLORS : C_COLORS;
            for (int j = 0; j < 4; ++j) 
			{
                if (s_nColorsChanged[i] & (1 << j)) 
                    SetPSConstant4fv(baseind+j, &lastRGBAfull[i][j][0]);
            }
            s_nColorsChanged[i] = 0;
        }
    }

    if (s_nTexDimsChanged) 
	{
        for (int i = 0; i < 8; ++i) 
		{
            if (s_nTexDimsChanged & (1<<i)) 
				SetPSTextureDims(i);         
        }
        s_nTexDimsChanged = 0;
    }

    if (s_bAlphaChanged) 
	{
        SetPSConstant4f(C_ALPHA, (lastAlpha&0xff)/255.0f, ((lastAlpha>>8)&0xff)/255.0f, 0, ((lastAlpha>>16)&0xff)/255.0f);
		s_bAlphaChanged = false;
    }

	if (s_bZTextureTypeChanged) 
	{    
        static float ffrac = 255.0f/256.0f;
        float ftemp[4];
        switch (bpmem.ztex2.type) 
		{
            case 0:
                // 8 bits
                // this breaks the menu in SSBM when it is set correctly to
                //ftemp[0] = ffrac/(65536.0f); ftemp[1] = 0; ftemp[2] = 0; ftemp[3] = 0;
                ftemp[0] = ffrac/65536.0f; ftemp[1] = ffrac/256.0f; ftemp[2] = ffrac; ftemp[3] = 0;
                break;
            case 1:
                // 16 bits
                ftemp[0] = ffrac/65536.0f; ftemp[1] = 0; ftemp[2] = 0; ftemp[3] = ffrac/256.0f;
                break;
            case 2:
                // 24 bits
                ftemp[0] = ffrac; ftemp[1] = ffrac/256.0f; ftemp[2] = ffrac/65536.0f; ftemp[3] = 0;
                break;
        }
		SetPSConstant4fv(C_ZBIAS, ftemp);
		s_bZTextureTypeChanged = false;
	}

	if (s_bZBiasChanged || s_bDepthRangeChanged) 
	{
        //ERROR_LOG("pixel=%x,%x, bias=%x\n", bpmem.zcontrol.pixel_format, bpmem.ztex2.type, lastZBias);
		// TODO : Should this use 16777216.0f or 16777215.0f ? 
        SetPSConstant4f(C_ZBIAS+1, lastDepthRange[0] / 16777216.0f, lastDepthRange[1] / 16777216.0f, 0, (float)( (((int)lastZBias<<8)>>8))/16777216.0f);
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

        if (s_nIndTexScaleChanged & 0x0c) {
            for (u32 i = 2; i < 4; ++i) {
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

                PRIM_LOG("indmtx%d: scale=%f, mat=(%f %f %f; %f %f %f)\n", i,
                    1024.0f*fscale, bpmem.indmtx[i].col0.ma * fscale, bpmem.indmtx[i].col1.mc * fscale, bpmem.indmtx[i].col2.me * fscale,
                    bpmem.indmtx[i].col0.mb * fscale, bpmem.indmtx[i].col1.md * fscale, bpmem.indmtx[i].col2.mf * fscale, fscale);
            }
        }
        s_nIndTexMtxChanged = 0;
    }

    if (s_bFogColorChanged) 
	{
        SetPSConstant4f(C_FOG, bpmem.fog.color.r / 255.0f, bpmem.fog.color.g / 255.0f, bpmem.fog.color.b / 255.0f, 0);
        s_bFogColorChanged = false;
    }

    if (s_bFogParamChanged) 
	{
        float a = bpmem.fog.a.GetA() * ((float)(1 << bpmem.fog.b_shift));
        float b = ((float)bpmem.fog.b_magnitude / 8388638) * ((float)(1 << (bpmem.fog.b_shift - 1)));
        SetPSConstant4f(C_FOG + 1, a, b, bpmem.fog.c_proj_fsel.GetC(), 0);
        s_bFogParamChanged = false;
    }

	for (int i = 0; i < 8; i++)
		lastCustomTexScale[i][0] = lastCustomTexScale[i][1] = 1.0f;
}

void PixelShaderManager::SetPSTextureDims(int texid)
{
	// non pow 2 textures - texdims.xy are the real texture dimensions used for wrapping
    // pow 2 textures - texdims.xy are reciprocals of the real texture dimensions
    // both - texdims.zw are the scaled dimensions
    float fdims[4];
	if (s_texturemask & (1 << texid))
	{
		TCoordInfo& tc = bpmem.texcoords[texid];
		fdims[0] = (float)(lastTexDims[texid] & 0xffff);
		fdims[1] = (float)((lastTexDims[texid] >> 16) & 0xfff);
        fdims[2] = (float)(tc.s.scale_minus_1 + 1)*lastCustomTexScale[texid][0];
		fdims[3] = (float)(tc.t.scale_minus_1 + 1)*lastCustomTexScale[texid][1];
	}
	else 
	{
        TCoordInfo& tc = bpmem.texcoords[texid];
		fdims[0] = 1.0f / (float)(lastTexDims[texid] & 0xffff);
		fdims[1] = 1.0f / (float)((lastTexDims[texid] >> 16) & 0xfff);
		fdims[2] = (float)(tc.s.scale_minus_1 + 1) * lastCustomTexScale[texid][0];
		fdims[3] = (float)(tc.t.scale_minus_1 + 1) * lastCustomTexScale[texid][1];
	}

	PRIM_LOG("texdims%d: %f %f %f %f\n", texid, fdims[0], fdims[1], fdims[2], fdims[3]);
	SetPSConstant4fv(C_TEXDIMS + texid, fdims);
}

// This one is high in profiles (0.5%)
void PixelShaderManager::SetColorChanged(int type, int num)
{
    int r = bpmem.tevregs[num].low.a;
	int a = bpmem.tevregs[num].low.b;
    int b = bpmem.tevregs[num].high.a;
	int g = bpmem.tevregs[num].high.b;
    float *pf = &lastRGBAfull[type][num][0];
    pf[0] = (float)r * (1.0f / 255.0f);
    pf[1] = (float)g * (1.0f / 255.0f);
    pf[2] = (float)b * (1.0f / 255.0f);
    pf[3] = (float)a * (1.0f / 255.0f);
    s_nColorsChanged[type] |= 1 << num;
    PRIM_LOG("pixel %scolor%d: %f %f %f %f\n", type?"k":"", num, pf[0], pf[1], pf[2], pf[3]);
}

void PixelShaderManager::SetAlpha(const AlphaFunc& alpha)
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

void PixelShaderManager::SetCustomTexScale(int texmapid, float x, float y)
{
	if (lastCustomTexScale[texmapid][0] != x || lastCustomTexScale[texmapid][1] != y)
	{
		s_nTexDimsChanged |= 1 << texmapid;
		lastCustomTexScale[texmapid][0] = x;
		lastCustomTexScale[texmapid][1] = y;
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

void PixelShaderManager::SetViewport(float* viewport)
{
	// reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
    // [0] = width/2
    // [1] = height/2
    // [2] = 16777215 * (farz - nearz)
    // [3] = xorig + width/2 + 342
    // [4] = yorig + height/2 + 342
    // [5] = 16777215 * farz

	if(lastDepthRange[0] != viewport[5] || lastDepthRange[1] != viewport[2])
	{
		lastDepthRange[0] = viewport[5];
		lastDepthRange[1] = viewport[2];

		s_bDepthRangeChanged = true;
	}
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

void PixelShaderManager::SetTexturesUsed(u32 nonpow2tex)
{
    if (s_texturemask != nonpow2tex)
	{
        for (int i = 0; i < 8; ++i)
		{
            if (nonpow2tex & (0x10101 << i))
			{
				// this check was previously implicit, but should it be here?
				if (s_nTexDimsChanged )
					s_nTexDimsChanged |= 1 << i;
            }
        }
        s_texturemask = nonpow2tex;
    }
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

void PixelShaderManager::SetColorMatrix(const float* pmatrix, const float* pfConstAdd)
{
    SetPSConstant4fv(C_COLORMATRIX,   pmatrix);
    SetPSConstant4fv(C_COLORMATRIX+1, pmatrix+4);
    SetPSConstant4fv(C_COLORMATRIX+2, pmatrix+8);
    SetPSConstant4fv(C_COLORMATRIX+3, pmatrix+12);
    SetPSConstant4fv(C_COLORMATRIX+4, pfConstAdd);
}

u32 PixelShaderManager::GetTextureMask()
{
	return s_texturemask;
}
