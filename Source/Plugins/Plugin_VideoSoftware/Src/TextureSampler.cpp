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


#include "TextureSampler.h"
#include "main.h"

#include "BPMemLoader.h"
#include "../../../Core/VideoCommon/Src/TextureDecoder.h"

#include <cmath>

namespace TextureSampler
{

inline int iround(float x)
{
    int t;

    __asm volatile
    {
        fld  x
        fistp t
    }

    return t;
}


inline void WrapCoord(int &coord, int wrapMode, int imageSize)
{
    switch (wrapMode)
    {
        case 0: // clamp
            coord = (coord>imageSize)?imageSize:(coord<0)?0:coord;
            break;
        case 1: // wrap
            coord = coord % (imageSize + 1);
            coord = (coord<0)?imageSize+coord:coord;
            break;
        case 2: // mirror
            {
                int sizePlus1 = imageSize + 1;
                int div = coord / sizePlus1;
                coord = coord - (div * sizePlus1);
                coord = (coord<0)?-coord:coord;
                coord = (div&1)?imageSize - coord:coord;
            }
            break;
    }
}

inline void SetTexel(u8 *inTexel, u32 *outTexel, u32 fract)
{
    outTexel[0] = inTexel[0] * fract;
    outTexel[1] = inTexel[1] * fract;
    outTexel[2] = inTexel[2] * fract;
    outTexel[3] = inTexel[3] * fract;
}

inline void AddTexel(u8 *inTexel, u32 *outTexel, u32 fract)
{
    outTexel[0] += inTexel[0] * fract;
    outTexel[1] += inTexel[1] * fract;
    outTexel[2] += inTexel[2] * fract;
    outTexel[3] += inTexel[3] * fract;
}

void Sample(float s, float t, float lod, u8 texmap, u8 *sample)
{
    FourTexUnits& texUnit = bpmem.tex[(texmap >> 2) & 1];
    u8 subTexmap = texmap & 3;

    TexMode0& tm0 = texUnit.texMode0[subTexmap];
    TexImage0& ti0 = texUnit.texImage0[subTexmap];
    TexTLUT& texTlut = texUnit.texTlut[subTexmap];

    u32 imageBase = texUnit.texImage3[subTexmap].image_base << 5;    
    u8 *imageSrc = g_VideoInitialize.pGetMemoryPointer(imageBase);

    bool linear = false;
    if (lod > 0 && tm0.min_filter > 4 || lod <= 0 && tm0.mag_filter)
        linear = true;

    if (linear)
    {
        s32 s256 = s32((s - 0.5f) * 256);
        s32 t256 = s32((t- 0.5f) * 256);

        int imageS = s256 >> 8;
        int imageSPlus1 = imageS + 1;
        u32 fractS = s256 & 0xff;
        fractS += fractS >> 7;

        int imageT = t256 >> 8;
        int imageTPlus1 = imageT + 1;
        u32 fractT = t256 & 0xff;
        fractT += fractT >> 7;

        u8 sampledTex[4];
        u32 texel[4];

        WrapCoord(imageS, tm0.wrap_s, ti0.width);
        WrapCoord(imageT, tm0.wrap_t, ti0.height);
        WrapCoord(imageSPlus1, tm0.wrap_s, ti0.width);
        WrapCoord(imageTPlus1, tm0.wrap_t, ti0.height);

        TexDecoder_DecodeTexel(sampledTex, imageSrc, imageS, imageT, ti0.width, ti0.format, texTlut.tmem_offset << 9, texTlut.tlut_format);
        SetTexel(sampledTex, texel, (256 - fractS) * (256 - fractT));

        TexDecoder_DecodeTexel(sampledTex, imageSrc, imageSPlus1, imageT, ti0.width, ti0.format, texTlut.tmem_offset << 9, texTlut.tlut_format);
        AddTexel(sampledTex, texel, (fractS) * (256 - fractT));

        TexDecoder_DecodeTexel(sampledTex, imageSrc, imageS, imageTPlus1, ti0.width, ti0.format, texTlut.tmem_offset << 9, texTlut.tlut_format);
        AddTexel(sampledTex, texel, (256 - fractS) * (fractT));

        TexDecoder_DecodeTexel(sampledTex, imageSrc, imageSPlus1, imageTPlus1, ti0.width, ti0.format, texTlut.tmem_offset << 9, texTlut.tlut_format);
        AddTexel(sampledTex, texel, (fractS) * (fractT));

        sample[0] = (u8)(texel[0] >> 16);
        sample[1] = (u8)(texel[1] >> 16);
        sample[2] = (u8)(texel[2] >> 16);
        sample[3] = (u8)(texel[3] >> 16);
    }
    else
    {
        int imageS = int(s);
        int imageT = int(t);

        WrapCoord(imageS, tm0.wrap_s, ti0.width);
        WrapCoord(imageT, tm0.wrap_t, ti0.height);

        TexDecoder_DecodeTexel(sample, imageSrc, imageS, imageT, ti0.width, ti0.format, texTlut.tmem_offset << 9, texTlut.tlut_format);   
    }
}

}
