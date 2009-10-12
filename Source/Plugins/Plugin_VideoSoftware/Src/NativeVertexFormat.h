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

#ifdef WIN32
#define LOADERDECL __cdecl
#else
#define LOADERDECL
#endif

typedef void (LOADERDECL *TPipelineFunction)();

struct InputVertexData
{
    u8 posMtx;
    u8 texMtx[8];

    float position[4];    
    float normal[3][3];
    u8 color[2][4];
    float texCoords[8][2];
};

struct OutputVertexData
{
    float mvPosition[3];
    float projectedPosition[4];
    float screenPosition[3];
    float normal[3][3];
    u8 color[2][4];
    float texCoords[8][3];

    void Lerp(float t, OutputVertexData *a, OutputVertexData *b)
    {
        #define LINTERP(T, OUT, IN) (OUT) + ((IN - OUT) * T)

        #define LINTERP_INT(T, OUT, IN) (OUT) + (((IN - OUT) * T) >> 8)

        for (int i = 0; i < 3; ++i)
            mvPosition[i] = LINTERP(t, a->mvPosition[i], b->mvPosition[i]);

        for (int i = 0; i < 4; ++i)
            projectedPosition[i] = LINTERP(t, a->projectedPosition[i], b->projectedPosition[i]);

        for (int i = 0; i < 3; ++i)
        {
            normal[i][0] = LINTERP(t, a->normal[i][0], b->normal[i][0]);
            normal[i][1] = LINTERP(t, a->normal[i][1], b->normal[i][1]);
            normal[i][2] = LINTERP(t, a->normal[i][2], b->normal[i][2]);
        }

        u16 t_int = (u16)(t * 256);
        for (int i = 0; i < 4; ++i)
        {
            color[0][i] = LINTERP_INT(t_int, a->color[0][i], b->color[0][i]);
            color[1][i] = LINTERP_INT(t_int, a->color[1][i], b->color[1][i]);
        }

        for (int i = 0; i < 8; ++i)
        {
            texCoords[i][0] = LINTERP(t, a->texCoords[i][0], b->texCoords[i][0]);
            texCoords[i][1] = LINTERP(t, a->texCoords[i][1], b->texCoords[i][1]);
            texCoords[i][2] = LINTERP(t, a->texCoords[i][2], b->texCoords[i][2]);
        }

        #undef LINTERP
        #undef LINTERP_INT
    }
};

#endif
