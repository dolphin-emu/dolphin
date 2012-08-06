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

#include <stdarg.h>
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

// TODO: Need packing?
struct uid_data
{
	u32 components;
	u32 numColorChans : 2;
	u32 numTexGens : 4;

	struct {
		u32 projection : 1; // XF_TEXPROJ_X
		u32 inputform : 2; // XF_TEXINPUT_X
		u32 texgentype : 3; // XF_TEXGEN_X
		u32 sourcerow : 5; // XF_SRCGEOM_X
		u32 embosssourceshift : 3; // what generated texcoord to use
		u32 embosslightshift : 3; // light index that is used
	} texMtxInfo[8];
	struct {
		u32 index : 6; // base row of dual transform matrix
		u32 normalize : 1; // normalize before send operation
	} postMtxInfo[8];
	struct {
		u32 enabled : 1;
	} dualTexTrans;
	struct {
		u32 matsource : 1;
		u32 enablelighting : 1;
		u32 ambsource : 1;
        u32 diffusefunc : 2;
        u32 attnfunc : 2;
		u32 light_mask : 8;
	} lit_chans[4];
};


class ShaderUid
{
public:
	ShaderUid()
	{
		memset(values, 0, sizeof(values));
	}

	void Write(const char* fmt, ...) {}
	const char* GetBuffer() { return NULL; }
	void SetBuffer(char* buffer) { }

	bool operator == (const ShaderUid& obj) const
	{
		return memcmp(this->values, obj.values, sizeof(values)) == 0;
	}

	// TODO: Store last frame used and order by that? makes much more sense anyway...
	bool operator < (const ShaderUid& obj) const
	{
		for (int i = 0; i < 24; ++i)
		{
			if (this->values[i] < obj.values[i])
				return true;
			else if (this->values[i] > obj.values[i])
				return false;
		}
		return false;
	}

	uid_data& GetUidData() { return data; }

private:
	union
	{
		uid_data data;
		u32 values[24]; // TODO: Length?
	};
};

class ShaderCode
{
public:
	ShaderCode() : buf(NULL), write_ptr(NULL)
	{

	}

	void Write(const char* fmt, ...)
	{
		va_list arglist;
		va_start(arglist, fmt);
		write_ptr += vsprintf(write_ptr, fmt, arglist);
		va_end(arglist);
	}

	const char* GetBuffer() { return buf; }
	void SetBuffer(char* buffer) { buf = buffer; write_ptr = buffer; }
	uid_data& GetUidData() { return *(uid_data*)NULL; }

private:
	const char* buf;
	char* write_ptr;
};

void GenerateShaderUid(ShaderUid& object, u32 components, API_TYPE api_type);
void GenerateShaderCode(ShaderCode& object, u32 components, API_TYPE api_type);


#endif // GCOGL_VERTEXSHADER_H
