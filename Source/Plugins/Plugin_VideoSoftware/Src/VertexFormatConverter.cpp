// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Common.h"

#include "VertexFormatConverter.h"


namespace VertexFormatConverter
{
    void LoadNormal1_Byte(InputVertexData *dst, u8 *src)
    {
        dst->normal[0][0] = (float)(s8)src[0] / 128;
        dst->normal[0][1] = (float)(s8)src[1] / 128;
        dst->normal[0][2] = (float)(s8)src[2] / 128;
    }

    void LoadNormal1_Short(InputVertexData *dst, u8 *src)
    {
        dst->normal[0][0] = (float)((s16*)src)[0] / 32768;
        dst->normal[0][1] = (float)((s16*)src)[1] / 32768;
        dst->normal[0][2] = (float)((s16*)src)[2] / 32768;
    }

    void LoadNormal1_Float(InputVertexData *dst, u8 *src)
    {
        dst->normal[0][0] = ((float*)src)[0];
        dst->normal[0][1] = ((float*)src)[1];
        dst->normal[0][2] = ((float*)src)[2];
    }

    void LoadNormal3_Byte(InputVertexData *dst, u8 *src)
    {
        for (int i = 0, j = 0; i < 3; i++, j+=3)
        {
            dst->normal[i][0] = (float)(s8)src[j + 0] / 128;
            dst->normal[i][1] = (float)(s8)src[j + 1] / 128;
            dst->normal[i][2] = (float)(s8)src[j + 2] / 128;
        }
    }

    void LoadNormal3_Short(InputVertexData *dst, u8 *src)
    {
        for (int i = 0, j = 0; i < 3; i++, j+=3)
        {
            dst->normal[i][0] = (float)((s16*)src)[j + 0] / 32768;
            dst->normal[i][1] = (float)((s16*)src)[j + 1] / 32768;
            dst->normal[i][2] = (float)((s16*)src)[j + 2] / 32768;
        }
    }

    void LoadNormal3_Float(InputVertexData *dst, u8 *src)
    {
        for (int i = 0, j = 0; i < 3; i++, j+=3)
        {
            dst->normal[i][0] = ((float*)src)[j + 0];
            dst->normal[i][1] = ((float*)src)[j + 1];
            dst->normal[i][2] = ((float*)src)[j + 2];
        }
    }
}