// Copyright (C) 2003-2008 Dolphin Project.

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

#define SHADER_POSMTX_ATTRIB 1
#define SHADER_NORM1_ATTRIB 6
#define SHADER_NORM2_ATTRIB 7


// shader variables
#define I_POSNORMALMATRIX "cpnmtx"
#define I_PROJECTION "cproj"
#define I_MATERIALS "cmtrl"
#define I_LIGHTS "clights"
#define I_TEXMATRICES "ctexmtx"
#define I_TRANSFORMMATRICES "ctrmtx"
#define I_NORMALMATRICES "cnmtx"
#define I_POSTTRANSFORMMATRICES "cpostmtx"
#define I_FOGPARAMS "cfog"

#define C_POSNORMALMATRIX 0
#define C_PROJECTION (C_POSNORMALMATRIX+6)
#define C_MATERIALS (C_PROJECTION+4)
#define C_LIGHTS (C_MATERIALS+4)
#define C_TEXMATRICES (C_LIGHTS+40)
#define C_TRANSFORMMATRICES (C_TEXMATRICES+24)
#define C_NORMALMATRICES (C_TRANSFORMMATRICES+64)
#define C_POSTTRANSFORMMATRICES (C_NORMALMATRICES+32)
#define C_FOGPARAMS (C_POSTTRANSFORMMATRICES+64)

char *GenerateVertexShader(u32 components, bool has_zbuffer_target);

#endif
