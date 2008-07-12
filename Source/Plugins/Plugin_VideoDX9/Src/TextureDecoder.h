#ifndef _TEXTUREDECODER_H
#define _TEXTUREDECODER_H

#include "Common.h"
#include "D3DBase.h"


enum 
{
	TMEM_SIZE = 1024*1024,
	HALFTMEM_SIZE = 512*1024
};
extern u8 texMem[TMEM_SIZE];


enum TextureFormat
{
    GX_TF_I4     = 0x0,
    GX_TF_I8     = 0x1,
    GX_TF_IA4    = 0x2,
    GX_TF_IA8    = 0x3,
    GX_TF_RGB565 = 0x4,
    GX_TF_RGB5A3 = 0x5,
    GX_TF_RGBA8  = 0x6,
    GX_TF_C4     = 0x8,
    GX_TF_C8     = 0x9,
    GX_TF_C14X2  = 0xA,
    GX_TF_CMPR   = 0xE,

	_GX_TF_CTF   = 0x20, /* copy-texture-format only */
    _GX_TF_ZTF   = 0x10, /* Z-texture-format */

	GX_TF_Z8     = 0x1 | _GX_TF_ZTF,
    GX_TF_Z16    = 0x3 | _GX_TF_ZTF,
    GX_TF_Z24X8  = 0x6 | _GX_TF_ZTF,

	//and the strange copy texture formats..
	

};

int TexDecoder_GetTexelSizeInNibbles(int format);
int TexDecoder_GetBlockWidthInTexels(int format);
int TexDecoder_GetPaletteSize(int fmt);

D3DFORMAT TexDecoder_Decode(u8 *dst, u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt);

#endif
