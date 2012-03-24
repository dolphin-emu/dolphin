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

#ifndef GCOGL_PIXELSHADER_H
#define GCOGL_PIXELSHADER_H

#include "VideoCommon.h"

#define I_COLORS      "color"
#define I_KCOLORS     "k"
#define I_ALPHA       "alphaRef"
#define I_TEXDIMS     "texdim"
#define I_ZBIAS       "czbias"
#define I_INDTEXSCALE "cindscale"
#define I_INDTEXMTX   "cindmtx"
#define I_FOG         "cfog"
#define I_PLIGHTS	  "cLights"
#define I_PMATERIALS   "cmtrl"

#define C_COLORMATRIX	0						// 0
#define C_COLORS		0						// 0
#define C_KCOLORS		(C_COLORS + 4)			// 4
#define C_ALPHA			(C_KCOLORS + 4)			// 8
#define C_TEXDIMS		(C_ALPHA + 1)			// 9
#define C_ZBIAS			(C_TEXDIMS + 8)			//17
#define C_INDTEXSCALE	(C_ZBIAS + 2)			//19
#define C_INDTEXMTX		(C_INDTEXSCALE + 2)		//21
#define C_FOG			(C_INDTEXMTX + 6)		//27

#define C_PLIGHTS		(C_FOG + 3)
#define C_PMATERIALS	(C_PLIGHTS + 40)
#define C_PENVCONST_END (C_PMATERIALS + 4)
#define PIXELSHADERUID_MAX_VALUES 70
#define PIXELSHADERUID_MAX_VALUES_SAFE 120

// DO NOT make anything in this class virtual.
template<bool safe>
class _PIXELSHADERUID
{
public:
	u32 values[safe ? PIXELSHADERUID_MAX_VALUES_SAFE : PIXELSHADERUID_MAX_VALUES];
	int num_values;

	_PIXELSHADERUID()
	{
	}

	_PIXELSHADERUID(const _PIXELSHADERUID& r)
	{
		num_values = r.num_values;
		if (safe) memcpy(values, r.values, PIXELSHADERUID_MAX_VALUES_SAFE);
		else memcpy(values, r.values, r.GetNumValues() * sizeof(values[0]));
	}

	int GetNumValues() const
	{
		if (safe) return (sizeof(values) / sizeof(u32));
		else return num_values;
	}

	bool operator <(const _PIXELSHADERUID& _Right) const
	{
		int N = GetNumValues();
		if (N < _Right.GetNumValues())
			return true;
		else if (N > _Right.GetNumValues())
			return false;
		for (int i = 0; i < N; ++i)
		{
			if (values[i] < _Right.values[i])
				return true;
			else if (values[i] > _Right.values[i])
				return false;
		}
		return false;
	}

	bool operator ==(const _PIXELSHADERUID& _Right) const
	{
		int N = GetNumValues();
		if (N != _Right.GetNumValues())
			return false;
		for (int i = 0; i < N; ++i)
		{
			if (values[i] != _Right.values[i])
				return false;
		}
		return true;
	}
};
typedef _PIXELSHADERUID<false> PIXELSHADERUID;
typedef _PIXELSHADERUID<true> PIXELSHADERUIDSAFE;

// Different ways to achieve rendering with destination alpha
enum DSTALPHA_MODE
{
	DSTALPHA_NONE, // Render normally, without destination alpha
	DSTALPHA_ALPHA_PASS, // Render normally first, then render again for alpha
	DSTALPHA_DUAL_SOURCE_BLEND // Use dual-source blending
};

const char *GeneratePixelShaderCode(DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components);

void GetPixelShaderId(PIXELSHADERUID *uid, DSTALPHA_MODE dstAlphaMode, u32 components);
void GetSafePixelShaderId(PIXELSHADERUIDSAFE *uid, DSTALPHA_MODE dstAlphaMode, u32 components);

// Used to make sure that our optimized pixel shader IDs don't lose any possible shader code changes
void ValidatePixelShaderIDs(API_TYPE api, PIXELSHADERUIDSAFE old_id, const std::string& old_code, DSTALPHA_MODE dstAlphaMode, u32 components);

#endif // GCOGL_PIXELSHADER_H
