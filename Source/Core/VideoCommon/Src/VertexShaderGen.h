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

#ifndef GCOGL_VERTEXSHADER_H
#define GCOGL_VERTEXSHADER_H

#include "XFMemory.h"
#include "VideoCommon.h"

#define SHADER_POSMTX_ATTRIB 1
#define SHADER_NORM1_ATTRIB  6
#define SHADER_NORM2_ATTRIB  7


// shader variables
#define I_POSNORMALMATRIX       "cpnmtx"
#define I_PROJECTION            "cproj"
#define I_MATERIALS             "cmtrl"
#define I_LIGHTS                "clights"
#define I_TEXMATRICES           "ctexmtx"
#define I_TRANSFORMMATRICES     "ctrmtx"
#define I_NORMALMATRICES        "cnmtx"
#define I_POSTTRANSFORMMATRICES "cpostmtx"
#define I_DEPTHPARAMS           "cDepth" // farZ, zRange, scaled viewport width, scaled viewport height

#define C_POSNORMALMATRIX        0
#define C_PROJECTION            (C_POSNORMALMATRIX + 6)
#define C_MATERIALS             (C_PROJECTION + 4)
#define C_LIGHTS                (C_MATERIALS + 4)
#define C_TEXMATRICES           (C_LIGHTS + 40)
#define C_TRANSFORMMATRICES     (C_TEXMATRICES + 24)
#define C_NORMALMATRICES        (C_TRANSFORMMATRICES + 64)
#define C_POSTTRANSFORMMATRICES (C_NORMALMATRICES + 32)
#define C_DEPTHPARAMS           (C_POSTTRANSFORMMATRICES + 64)
#define C_VENVCONST_END			(C_DEPTHPARAMS + 4)

template<bool safe>
class _VERTEXSHADERUID
{
#define NUM_VSUID_VALUES_SAFE 25
public:
	u32 values[safe ? NUM_VSUID_VALUES_SAFE : 9];

	_VERTEXSHADERUID()
	{
	}

	_VERTEXSHADERUID(const _VERTEXSHADERUID& r)
	{
		for (size_t i = 0; i < sizeof(values) / sizeof(u32); ++i) 
			values[i] = r.values[i]; 
	}

	int GetNumValues() const 
	{
		if (safe) return NUM_VSUID_VALUES_SAFE;
		else return (((values[0] >> 23) & 0xf) * 3 + 3) / 4 + 3; // numTexGens*3/4+1
	}

	bool operator <(const _VERTEXSHADERUID& _Right) const
	{
		if (values[0] < _Right.values[0])
			return true;
		else if (values[0] > _Right.values[0])
			return false;
		int N = GetNumValues();
		for (int i = 1; i < N; ++i) 
		{
			if (values[i] < _Right.values[i])
				return true;
			else if (values[i] > _Right.values[i])
				return false;
		}
		return false;
	}

	bool operator ==(const _VERTEXSHADERUID& _Right) const
	{
		if (values[0] != _Right.values[0])
			return false;
		int N = GetNumValues();
		for (int i = 1; i < N; ++i)
		{
			if (values[i] != _Right.values[i])
				return false;
		}
		return true;
	}
};
typedef _VERTEXSHADERUID<false> VERTEXSHADERUID;
typedef _VERTEXSHADERUID<true> VERTEXSHADERUIDSAFE;


// components is included in the uid.
char* GenerateVSOutputStruct(char* p, u32 components, API_TYPE api_type);
const char *GenerateVertexShaderCode(u32 components, API_TYPE api_type);

void GetVertexShaderId(VERTEXSHADERUID *uid, u32 components);
void GetSafeVertexShaderId(VERTEXSHADERUIDSAFE *uid, u32 components);

// Used to make sure that our optimized vertex shader IDs don't lose any possible shader code changes
void ValidateVertexShaderIDs(API_TYPE api, VERTEXSHADERUIDSAFE old_id, const std::string& old_code, u32 components);

#endif // GCOGL_VERTEXSHADER_H
