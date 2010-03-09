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

#ifndef _NATIVEVERTEXFORMAT_H
#define _NATIVEVERTEXFORMAT_H

#include "../../Plugin_VideoDX9/Src/Vec3.h"

#ifdef WIN32
#define LOADERDECL __cdecl
#else
#define LOADERDECL
#endif

typedef void (LOADERDECL *TPipelineFunction)();

struct Vec4
{
	float x;
	float y;
	float z;
	float w;
};

struct InputVertexData
{
    u8 posMtx;
    u8 texMtx[8];

    Vec3 position;    
    Vec3 normal[3];
    u8 color[2][4];
    float texCoords[8][2];
};

struct OutputVertexData
{
    Vec3 mvPosition;
    Vec4 projectedPosition;
    Vec3 screenPosition;
    Vec3 normal[3];
    u8 color[2][4];
    Vec3 texCoords[8];

    void Lerp(float t, OutputVertexData *a, OutputVertexData *b)
    {
        #define LINTERP(T, OUT, IN) (OUT) + ((IN - OUT) * T)

        #define LINTERP_INT(T, OUT, IN) (OUT) + (((IN - OUT) * T) >> 8)

        mvPosition = LINTERP(t, a->mvPosition, b->mvPosition);

        projectedPosition.x = LINTERP(t, a->projectedPosition.x, b->projectedPosition.x);
		projectedPosition.y = LINTERP(t, a->projectedPosition.y, b->projectedPosition.y);
		projectedPosition.z = LINTERP(t, a->projectedPosition.z, b->projectedPosition.z);
		projectedPosition.w = LINTERP(t, a->projectedPosition.w, b->projectedPosition.w);

        for (int i = 0; i < 3; ++i)
        {
            normal[i] = LINTERP(t, a->normal[i], b->normal[i]);
        }

        u16 t_int = (u16)(t * 256);
        for (int i = 0; i < 4; ++i)
        {
            color[0][i] = LINTERP_INT(t_int, a->color[0][i], b->color[0][i]);
            color[1][i] = LINTERP_INT(t_int, a->color[1][i], b->color[1][i]);
        }

        for (int i = 0; i < 8; ++i)
        {
            texCoords[i] = LINTERP(t, a->texCoords[i], b->texCoords[i]);
        }

        #undef LINTERP
        #undef LINTERP_INT
    }
};

#endif
