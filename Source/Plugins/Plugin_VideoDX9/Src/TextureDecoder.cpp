#include "stdafx.h"
#include "D3DBase.h"

#include "main.h"
#include "Utils.h"

#include "BPStructs.h"
#include "TextureDecoder.h"

#include "OpcodeDecoding.h"


// TRAM
u8 texMem[TMEM_SIZE];

//////////////////////////////////////////////////////////////////////////
// Gamecube texture decoder
//////////////////////////////////////////////////////////////////////////
// Decodes all known Gamecube texture formats.
// TODO - speedup
// by ector
//////////////////////////////////////////////////////////////////////////
int TexDecoder_GetTexelSizeInNibbles(int format)
{
	switch(format) {
	case GX_TF_I4: return 1;   
	case GX_TF_I8: return 2;
	case GX_TF_IA4: return 2;
	case GX_TF_IA8: return 4;
	case GX_TF_RGB565: return 4;
	case GX_TF_RGB5A3: return 4;
	case GX_TF_RGBA8: return 8;
	case GX_TF_C4: return 1;
	case GX_TF_C8: return 2;
	case GX_TF_C14X2: return 4;
	case GX_TF_CMPR: return 1;
	default: return 1;
	}
}
int TexDecoder_GetBlockWidthInTexels(int format)
{
	switch(format) {
	case GX_TF_I4: return 8;   
	case GX_TF_I8: return 8;
	case GX_TF_IA4: return 8;
	case GX_TF_IA8: return 4;
	case GX_TF_RGB565: return 4;
	case GX_TF_RGB5A3: return 4;
	case GX_TF_RGBA8: return  4;
	case GX_TF_C4: return 8;
	case GX_TF_C8: return 8;
	case GX_TF_C14X2: return 4;
	case GX_TF_CMPR: return 8;
	default:return 8;
	}
}

//returns bytes
int TexDecoder_GetPaletteSize(int format)
{
	switch (format) {
	case GX_TF_C4: return 16*2;
	case GX_TF_C8: return 256*2;
	case GX_TF_C14X2: return 16384*2;
	default:
		return 0;
	}
}

inline u32 decode565(u16 val)
{
	int r,g,b,a;
	r=lut5to8[(val>>11)&0x1f];
	g=lut6to8[(val>>5 )&0x3f];
	b=lut5to8[(val    )&0x1f];
	a=0xFF;
	return (a<<24) | (r<<16) | (g<<8) | b;
}

inline u32 decodeIA8(u16 val)
{
	int a=val>>8;
	int r,g,b;
	r=g=b=val&0xFF;
	return (a<<24) | (r<<16) | (g<<8) | b;
}

inline u32 decode5A3(u16 val)
{
	int r,g,b,a;
	if ((val&0x8000))
	{
		r=lut5to8[(val>>10)&0x1f];
		g=lut5to8[(val>>5 )&0x1f];
		b=lut5to8[(val    )&0x1f];
		a=0xFF;
	}
	else
	{
		a=lut3to8[(val>>12)&0x7];
		r=lut4to8[(val>>8 )&0xf];
		g=lut4to8[(val>>4 )&0xf];
		b=lut4to8[(val    )&0xf];
	}
	return (a<<24) | (r<<16) | (g<<8) | b;
}



struct DXTBlock
{
	u16 color1;
	u16 color2;
	u8 lines[4];
};

inline int expand8888(const int j)
{
	int i = j | (j<<8);
	return i|(i<<16);
}

void decodebytesI4(u32 *dst, u8 *src, int numbytes)
{
	for (int x=0; x<numbytes; x++)
	{
		int val = src[x];
		*dst++ = expand8888(lut4to8[val>>4]);
		*dst++ = expand8888(lut4to8[val&15]);
	}
}

void decodebytesI8(u32 *dst, u8 *src, int numbytes)
{
	for (int x=0; x<numbytes; x++)
		*dst++ = expand8888(src[x]);   //asm bswap loop?
}

void decodebytesC4(u32 *dst, u8 *src, int numbytes, int tlutaddr, int tlutfmt)
{
	u16 *tlut = (u16*)(texMem+tlutaddr);
	for (int x=0; x<numbytes; x++)
	{
		int val = src[x];
		switch(tlutfmt) {
		case 0:
			*dst++ = decodeIA8(_byteswap_ushort(tlut[(val>>4)]));
			*dst++ = decodeIA8(_byteswap_ushort(tlut[(val&15)]));
			break;
		case 1:
			*dst++ = decode565(_byteswap_ushort(tlut[(val>>4)]));
			*dst++ = decode565(_byteswap_ushort(tlut[(val&15)]));
			break;
		case 2:
			*dst++ = decode5A3(_byteswap_ushort(tlut[(val>>4)]));
			*dst++ = decode5A3(_byteswap_ushort(tlut[(val&15)]));
			break;
		case 3: //ERROR
			*dst++ = 0xFFFF00FF;
			*dst++ = 0xFFFF00FF;
			break;
		}
	}
}

void decodebytesC8(u32 *dst, u8 *src, int numbytes, int tlutaddr, int tlutfmt)
{ 
	u16 *tlut = (u16*)(texMem+tlutaddr);
	for (int x=0; x<numbytes; x++)
	{
		int val = src[x];
		switch(tlutfmt) {
		case 0:
			*dst++ = decodeIA8(_byteswap_ushort(tlut[val]));
			break;
		case 1:
			*dst++ = decode565(_byteswap_ushort(tlut[val]));
			break;
		case 2:
			*dst++ = decode5A3(_byteswap_ushort(tlut[val]));
			break;
		case 3: //ERROR
			*dst++ = 0xFFFF00FF;
			break;
		}
	}
}


void decodebytesC14X2(u32 *dst, u16 *src, int numpixels, int tlutaddr, int tlutfmt)
{
	u16 *tlut = (u16*)(texMem+tlutaddr);
	for (int x=0; x<numpixels; x++)
	{
		int val = _byteswap_ushort(src[x]);
		switch(tlutfmt) {
		case 0:
			*dst++ = decodeIA8(_byteswap_ushort(tlut[(val&0x3FFF)]));
			break;
		case 1:
			*dst++ = decode565(_byteswap_ushort(tlut[(val&0x3FFF)]));
			break;
		case 2:
			*dst++ = decode5A3(_byteswap_ushort(tlut[(val&0x3FFF)]));
			break;
		case 3: //ERROR
			*dst++ = 0xFFFF00FF;
			break;
		}
	}
}

void decodebytesRGB565(u32 *dst, u16 *src, int numpixels)
{
	for (int x=0; x<numpixels; x++)
		*dst++ = decode565(_byteswap_ushort(src[x]));
}

void decodebytesIA4(u32 *dst, u8 *src, int numbytes)
{
	for (int x=0; x<numbytes; x++)
	{
		int val = src[x];

		int a = lut4to8[val>>4];
		int r = lut4to8[val&15];
		*dst++ = (a<<24) | (r<<16) | (r<<8) | r;
	}
}

inline void decodebytesIA8(u32 *dst, u16 *src, int numpixels)
{
	for (int x=0; x<numpixels; x++)
		*dst++ = decodeIA8(_byteswap_ushort(src[x]));
}

inline void decodebytesRGB5A3(u32 *dst, u16 *src, int numpixels)
{
	for (int x=0; x<numpixels; x++)
		*dst++ = decode5A3(_byteswap_ushort(src[x]));
}

void decodebytesARGB8pass1(u32 *dst, u16 *src, int numpixels)
{
	for (int x=0; x<numpixels; x++)
	{
		int val = _byteswap_ushort(src[x]);
		int a=val&0xFF;
		val>>=8;

		*dst++ = (a<<16) | (val<<24);
	}
}
void decodebytesARGB8pass2(u32 *dst, u16 *src, int numpixels)
{
	for (int x=0; x<numpixels; x++)
	{
		int val = _byteswap_ushort(src[x]);
		int a=val&0xFF;
		val>>=8;

		*dst++ |= (val<<8) | (a<<0);
	}
}

inline u32 makecol(int r,int g,int b,int a)
{
	return ((a&255)<<24)|((r&255)<<16)|((g&255)<<8)|((b&255));
}


//this needs to be FAST, used by some games realtime video
//TODO: port to ASM or intrinsics
void decodeDXTBlock(u32 *dst, DXTBlock *src, int pitch)
{
	u16 c1 = _byteswap_ushort(src->color1);
	u16 c2 = _byteswap_ushort(src->color2);
	int blue1 = lut5to8[c1&0x1F];
	int blue2 = lut5to8[c2&0x1F];
	int green1 = lut6to8[(c1>>5)&0x3F];
	int green2 = lut6to8[(c2>>5)&0x3F];
	int red1 = lut5to8[(c1>>11)&0x1F];
	int red2 = lut5to8[(c2>>11)&0x1F];
	
	int colors[4];

	if (c1>c2)
	{
		colors[0]=makecol(red1,green1,blue1,255);
		colors[1]=makecol(red2,green2,blue2,255);
		colors[2]=makecol(red1+(red2-red1)/3,green1+(green2-green1)/3,blue1+(blue2-blue1)/3,255);
		colors[3]=makecol(red2+(red1-red2)/3,green2+(green1-green2)/3,blue2+(blue1-blue2)/3,255);
	}
	else
	{
		colors[0]=makecol(red1,green1,blue1,255);
		colors[1]=makecol(red2,green2,blue2,255);
		colors[2]=makecol((red1+red2)/2,(green1+green2)/2,(blue1+blue2)/2,255);
		colors[3]=makecol(0,0,0,0); //transparent
	}

    for (int y=0; y<4; y++)
	{ 
		int val = src->lines[y];
		for (int x=0; x<4; x++)
		{
			dst[x] = colors[(val>>6)&3];
			val<<=2;
		}
		dst+=pitch;
	}
}


//switch endianness, unswizzle
//TODO: to save memory, don't blindly convert everything to argb8888
//also ARGB order needs to be swapped later, to accommodate modern hardware better
//need to add DXT support too
D3DFORMAT TexDecoder_Decode(u8 *dst, u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt)
{
    DVSTARTPROFILE();

	switch (texformat)
	{
	case GX_TF_C4:
		{
			for (int y=0; y<height; y+=8)
				for (int x=0; x<width; x+=8)
					for (int iy=0; iy<8; iy++, src+=4)
						decodebytesC4((u32*)dst+(y+iy)*width+x,src,4,tlutaddr,tlutfmt);
		}
		return D3DFMT_A8R8G8B8;
	case GX_TF_I4:
		{
			for (int y=0; y<height; y+=8)
				for (int x=0; x<width; x+=8)
					for (int iy=0; iy<8; iy++, src+=4)
						decodebytesI4((u32*)dst+(y+iy)*width+x,src,4);
		}
		return D3DFMT_A8R8G8B8;
	case GX_TF_C8:
		{
			for (int y=0; y<height; y+=4)
				for (int x=0; x<width; x+=8)
					for (int iy=0; iy<4; iy++, src+=8)
						decodebytesC8((u32*)dst+(y+iy)*width+x,src,8,tlutaddr,tlutfmt);
		}
		return D3DFMT_A8R8G8B8;
	case GX_TF_I8:
		{
			for (int y=0; y<height; y+=4)
				for (int x=0; x<width; x+=8)
					for (int iy=0; iy<4; iy++, src+=8)
						decodebytesI8((u32*)dst+(y+iy)*width+x,src,8);
		}
		return D3DFMT_A8R8G8B8;
	case GX_TF_IA4:
		{
			for (int y=0; y<height; y+=4)
				for (int x=0; x<width; x+=8)
					for (int iy=0; iy<4; iy++, src+=8)
						decodebytesIA4((u32*)dst+(y+iy)*width+x,src,8);
		}
		return D3DFMT_A8R8G8B8;
	case GX_TF_IA8:
		{
			for (int y=0; y<height; y+=4)
				for (int x=0; x<width; x+=4)
					for (int iy=0; iy<4; iy++,src+=8)
						decodebytesIA8((u32*)dst+(y+iy)*width+x,(u16*)src,4);
		}
		return D3DFMT_A8R8G8B8;
	case GX_TF_C14X2: 
		{
			for (int y=0; y<height; y+=4)
				for (int x=0; x<width; x+=4)
					for (int iy=0; iy<4; iy++,src+=8)
						decodebytesC14X2((u32*)dst+(y+iy)*width+x,(u16*)src,4,tlutaddr,tlutfmt);
		}
		return D3DFMT_A8R8G8B8;
	case GX_TF_RGB565:
		{
			for (int y=0; y<height; y+=4)
				for (int x=0; x<width; x+=4)
					for (int iy=0; iy<4; iy++,src+=8)
						decodebytesRGB565((u32*)dst+(y+iy)*width+x,(u16*)src,4);
		}
		return D3DFMT_A8R8G8B8;
	case GX_TF_RGB5A3:
		{
			for (int y=0; y<height; y+=4)
				for (int x=0; x<width; x+=4)
					for (int iy=0; iy<4; iy++,src+=8)
						decodebytesRGB5A3((u32*)dst+(y+iy)*width+x,(u16*)src,4);
		}
		return D3DFMT_A8R8G8B8;
	case GX_TF_RGBA8:
		{
			for (int y=0; y<height; y+=4)
				for (int x=0; x<width; x+=4)
				{
					for (int iy=0; iy<4; iy++,src+=8)
						decodebytesARGB8pass1((u32*)dst+(y+iy)*width+x,(u16*)src,4);
					for (int iy=0; iy<4; iy++,src+=8)
						decodebytesARGB8pass2((u32*)dst+(y+iy)*width+x,(u16*)src,4);
				}
		}
		return D3DFMT_A8R8G8B8;
	case GX_TF_CMPR:
		{
			// 11111111 22222222 55555555 66666666
			// 33333333 44444444 77777777 88888888

			
			for (int y=0; y<height; y+=8)
				for (int x=0; x<width; x+=8)
				{
					decodeDXTBlock((u32*)dst+y*width+x,(DXTBlock*)src,width);src+=sizeof(DXTBlock);
					decodeDXTBlock((u32*)dst+y*width+x+4,(DXTBlock*)src,width);src+=sizeof(DXTBlock);
					decodeDXTBlock((u32*)dst+(y+4)*width+x,(DXTBlock*)src,width);src+=sizeof(DXTBlock);
					decodeDXTBlock((u32*)dst+(y+4)*width+x+4,(DXTBlock*)src,width);src+=sizeof(DXTBlock);
				}
			
			/*
			int rowlen = width/2;
			int numBigBlockRows = height/8;
			int numBigBlocksPerRow = width/8;
			int numSmallBlocksPerRow = width/4;
			int numSmallBlockRows = height/4;
			int smallBlockSize = 8;
			for (int y=0; y<height/8; y+=8)
			{
				for (int x=0; x<width/8; x+=8)
				{
					memcpy(dst+y*2*numSmallBlocksPerRow*smallBlockSize+x*2*smallBlockSize,src,smallBlockSize); src+=smallBlockSize;
					memcpy(dst+y*2*numSmallBlocksPerRow*smallBlockSize+(x*2+1)*smallBlockSize,src,smallBlockSize); src+=smallBlockSize;
					memcpy(dst+(y*2+1)*numSmallBlocksPerRow*smallBlockSize+x*2*smallBlockSize,src,smallBlockSize); src+=smallBlockSize;
					memcpy(dst+(y*2+1)*numSmallBlocksPerRow*smallBlockSize+(x*2+1)*smallBlockSize,src,smallBlockSize); src+=smallBlockSize;
				}
			}*/
			
		}
		return D3DFMT_A8R8G8B8;
//		return D3DFMT_DXT1;
	}
	return D3DFMT_UNKNOWN;
}