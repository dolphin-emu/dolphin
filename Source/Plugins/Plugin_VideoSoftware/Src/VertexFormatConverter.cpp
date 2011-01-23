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
	// This fracs are fixed acording to format
#define S8FRAC 0.015625f; // 1.0f / (1U << 6)
#define S16FRAC 0.00006103515625f; // 1.0f / (1U << 14)

    void LoadNormal1_Byte(InputVertexData *dst, u8 *src)
    {
        dst->normal[0].x = ((s8)src[0]) * S8FRAC;
        dst->normal[0].y = ((s8)src[1]) * S8FRAC;
        dst->normal[0].z = ((s8)src[2]) * S8FRAC;
    }

    void LoadNormal1_Short(InputVertexData *dst, u8 *src)
    {
        dst->normal[0].x = ((s16*)src)[0] * S16FRAC;
        dst->normal[0].y = ((s16*)src)[1] * S16FRAC;
        dst->normal[0].z = ((s16*)src)[2] * S16FRAC;
    }

    void LoadNormal1_Float(InputVertexData *dst, u8 *src)
    {
        dst->normal[0].x = ((float*)src)[0];
        dst->normal[0].y = ((float*)src)[1];
        dst->normal[0].z = ((float*)src)[2];
    }

    void LoadNormal3_Byte(InputVertexData *dst, u8 *src)
    {
        for (int i = 0, j = 0; i < 3; i++, j+=3)
        {
            dst->normal[i].x = ((s8)src[j + 0]) * S8FRAC;
            dst->normal[i].y = ((s8)src[j + 1]) * S8FRAC;
            dst->normal[i].z = ((s8)src[j + 2]) * S8FRAC;
        }
    }

    void LoadNormal3_Short(InputVertexData *dst, u8 *src)
    {
        for (int i = 0, j = 0; i < 3; i++, j+=3)
        {
            dst->normal[i].x = ((s16*)src)[j + 0] * S16FRAC;
            dst->normal[i].y = ((s16*)src)[j + 1] * S16FRAC;
            dst->normal[i].z = ((s16*)src)[j + 2] * S16FRAC;
        }
    }

    void LoadNormal3_Float(InputVertexData *dst, u8 *src)
    {
        for (int i = 0, j = 0; i < 3; i++, j+=3)
        {
            dst->normal[i].x = ((float*)src)[j + 0];
            dst->normal[i].y = ((float*)src)[j + 1];
            dst->normal[i].z = ((float*)src)[j + 2];
        }
    }
}
