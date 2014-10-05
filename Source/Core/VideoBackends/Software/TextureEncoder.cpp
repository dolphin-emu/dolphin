// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoBackends/Software/BPMemLoader.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/TextureEncoder.h"

#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder.h"


namespace TextureEncoder
{

static inline void RGBA_to_RGBA8(const u8 *src, u8* r, u8* g, u8* b, u8* a)
{
	u32 srcColor = *(u32*)src;
	*a = Convert6To8(srcColor & 0x3f);
	*b = Convert6To8((srcColor >> 6) & 0x3f);
	*g = Convert6To8((srcColor >> 12)& 0x3f);
	*r = Convert6To8((srcColor >> 18)& 0x3f);
}

static inline void RGBA_to_RGB8(const u8 *src, u8* r, u8* g, u8* b)
{
	u32 srcColor = *(u32*)src;
	*b = Convert6To8((srcColor >> 6) & 0x3f);
	*g = Convert6To8((srcColor >> 12)& 0x3f);
	*r = Convert6To8((srcColor >> 18)& 0x3f);
}

static inline u8 RGB8_to_I(u8 r, u8 g, u8 b)
{
	// values multiplied by 256 to keep math integer
	u16 val = 4096 + 66 * r + 129 * g + 25 * b;
	return val >> 8;
}

// box filter sampling averages 4 samples with the source texel being the top left of the box
// components are scaled to the range 0-255 after all samples are taken

static inline void BoxfilterRGBA_to_RGBA8(const u8* src, u8* r, u8* g, u8* b, u8* a)
{
	u16 r16 = 0, g16 = 0, b16 = 0, a16 = 0;

	for (int y = 0; y < 2; y++)
	{
		for (int x = 0; x < 2; x++)
		{
			u32 srcColor = *(u32*)src;

			a16 += srcColor & 0x3f;
			b16 += (srcColor >> 6) & 0x3f;
			g16 += (srcColor >> 12) & 0x3f;
			r16 += (srcColor >> 18) & 0x3f;

			src += 3; // move to next pixel
		}
		src += (640 - 2) * 3; // move to next line
	}

	*r = r16 + (r16 >> 6);
	*g = g16 + (g16 >> 6);
	*b = b16 + (b16 >> 6);
	*a = a16 + (a16 >> 6);
}

static inline void BoxfilterRGBA_to_RGB8(const u8* src, u8* r, u8* g, u8* b)
{
	u16 r16 = 0, g16 = 0, b16 = 0;

	for (int y = 0; y < 2; y++)
	{
		for (int x = 0; x < 2; x++)
		{
			u32 srcColor = *(u32*)src;

			b16 += (srcColor >> 6) & 0x3f;
			g16 += (srcColor >> 12) & 0x3f;
			r16 += (srcColor >> 18) & 0x3f;

			src += 3; // move to next pixel
		}
		src += (640 - 2) * 3; // move to next line
	}

	*r = r16 + (r16 >> 6);
	*g = g16 + (g16 >> 6);
	*b = b16 + (b16 >> 6);
}

static inline void BoxfilterRGBA_to_x8(const u8* src, u8* x8, int shift)
{
	u16 x16 = 0;

	for (int y = 0; y < 2; y++)
	{
		for (int x = 0; x < 2; x++)
		{
			u32 srcColor = *(u32*)src;

			x16 += (srcColor >> shift) & 0x3f;

			src += 3; // move to next pixel
		}
		src += (640 - 2) * 3; // move to next line
	}

	*x8 = x16 + (x16 >> 6);
}

static inline void BoxfilterRGBA_to_xx8(const u8* src, u8* x1, u8* x2, int shift1, int shift2)
{
	u16 x16_1 = 0;
	u16 x16_2 = 0;

	for (int y = 0; y < 2; y++)
	{
		for (int x = 0; x < 2; x++)
		{
			u32 srcColor = *(u32*)src;

			x16_1 += (srcColor >> shift1) & 0x3f;
			x16_2 += (srcColor >> shift2) & 0x3f;

			src += 3; // move to next pixel
		}
		src += (640 - 2) * 3; // move to next line
	}

	*x1 = x16_1 + (x16_1 >> 6);
	*x2 = x16_2 + (x16_2 >> 6);
}

static inline void BoxfilterRGB_to_RGB8(const u8* src, u8* r, u8* g, u8* b)
{
	u16 r16 = 0, g16 = 0, b16 = 0;

	for (int y = 0; y < 2; y++)
	{
		for (int x = 0; x < 2; x++)
		{
			b16 += src[0];
			g16 += src[1];
			r16 += src[2];

			src += 3; // move to next pixel
		}
		src += (640 - 2) * 3; // move to next line
	}

	*r = r16 >> 2;
	*g = g16 >> 2;
	*b = b16 >> 2;
}

static inline void BoxfilterRGB_to_x8(u8 *src, u8* x8, int comp)
{
	u16 x16 = 0;

	for (int y = 0; y < 2; y++)
	{
		for (int x = 0; x < 2; x++)
		{

			x16 += src[comp];

			src += 3; // move to next pixel
		}
		src += (640 - 2) * 3; // move to next line
	}

	*x8 = x16 >> 2;
}

static inline void BoxfilterRGB_to_xx8(const u8* src, u8* x1, u8* x2, int comp1, int comp2)
{
	u16 x16_1 = 0;
	u16 x16_2 = 0;

	for (int y = 0; y < 2; y++)
	{
		for (int x = 0; x < 2; x++)
		{

			x16_1 += src[comp1];
			x16_2 += src[comp2];

			src += 3; // move to next pixel
		}
		src += (640 - 2) * 3; // move to next line
	}

	*x1 = x16_1 >> 2;
	*x2 = x16_2 >> 2;
}

static void SetBlockDimensions(int blkWidthLog2, int blkHeightLog2, u16* sBlkCount, u16* tBlkCount, u16* sBlkSize, u16* tBlkSize)
{
	// if half_scale is 1 then the size is cut in half
	u32 width = bpmem.copyTexSrcWH.x >> bpmem.triggerEFBCopy.half_scale;
	u32 height = bpmem.copyTexSrcWH.y >> bpmem.triggerEFBCopy.half_scale;

	*sBlkCount = (width >> blkWidthLog2) + 1;
	*tBlkCount = (height >> blkHeightLog2) + 1;

	*sBlkSize = 1 << blkWidthLog2;
	*tBlkSize = 1 << blkHeightLog2;
}

static void SetSpans(int sBlkSize, int tBlkSize, s32* tSpan, s32* sBlkSpan, s32* tBlkSpan, s32* writeStride)
{
	// width is 1 less than the number of pixels of width
	u32 width = bpmem.copyTexSrcWH.x >> bpmem.triggerEFBCopy.half_scale;
	u32 alignedWidth = (width + sBlkSize) & (~(sBlkSize-1));

	u32 readStride = 3 << bpmem.triggerEFBCopy.half_scale;

	*tSpan = (640 - sBlkSize) * readStride; // bytes to advance src pointer after each row of texels in a block
	*sBlkSpan = ((-640 * tBlkSize) + sBlkSize) * readStride; // bytes to advance src pointer after each block
	*tBlkSpan = ((640 * tBlkSize) - alignedWidth) * readStride; // bytes to advance src pointer after each row of blocks

	*writeStride = bpmem.copyMipMapStrideChannels * 32;
}

#define ENCODE_LOOP_BLOCKS									\
		for (int tBlk = 0; tBlk < tBlkCount; tBlk++) {		\
			dst = dstBlockStart;							\
			for (int sBlk = 0; sBlk < sBlkCount; sBlk++) {	\
				for (int t = 0; t < tBlkSize; t++) {		\
					for (int s = 0; s < sBlkSize; s++) {	\


#define ENCODE_LOOP_SPANS									\
					}										\
					src += tSpan;							\
				}											\
				src += sBlkSpan;							\
			}												\
			src += tBlkSpan;								\
			dstBlockStart += writeStride;					\
		}													\

#define ENCODE_LOOP_SPANS2									\
					}										\
					src += tSpan;							\
				}											\
				src += sBlkSpan;							\
				dst += 32;									\
			}												\
			src += tBlkSpan;								\
			dstBlockStart += writeStride;					\
		}													\

static void EncodeRGBA6(u8 *dst, u8 *src, u32 format)
{
	u16 sBlkCount, tBlkCount, sBlkSize, tBlkSize;
	s32 tSpan, sBlkSpan, tBlkSpan, writeStride;
	u8 r, g, b, a;
	u32 readStride = 3;
	u8 *dstBlockStart = dst;

	switch (format)
	{
	case GX_TF_I4:
		SetBlockDimensions(3, 3, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		sBlkSize /= 2;
		ENCODE_LOOP_BLOCKS
		{
			RGBA_to_RGB8(src, &r, &g, &b);
			src += readStride;
			*dst = RGB8_to_I(r, g, b) & 0xf0;

			RGBA_to_RGB8(src, &r, &g, &b);
			src += readStride;
			*dst |= RGB8_to_I(r, g, b) >> 4;
			dst++;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_I8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			RGBA_to_RGB8(src, &r, &g, &b);
			src += readStride;
			*dst++ = RGB8_to_I(r, g, b);
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_IA4:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			RGBA_to_RGBA8(src, &r, &g, &b, &a);
			src += readStride;
			*dst++ = (a & 0xf0) | (RGB8_to_I(r, g, b) >> 4);
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_IA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			RGBA_to_RGBA8(src, &r, &g, &b, &a);
			src += readStride;
			*dst++ = a;
			*dst++ = RGB8_to_I(r, g, b);
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGB565:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			src += readStride;

			u16 val = ((srcColor >> 8) & 0xf800) | ((srcColor >> 7) & 0x07e0) | ((srcColor >> 7) & 0x001e);
			*(u16*)dst = Common::swap16(val);
			dst+=2;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGB5A3:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			src += readStride;

			u16 alpha = (srcColor << 9) & 0x7000;
			u16 val;
			if (alpha == 0x7000) // 555
				val = 0x8000 | ((srcColor >> 9) & 0x7c00) | ((srcColor >> 8) & 0x03e0) | ((srcColor >> 7) & 0x001e);
			else // 4443
				val = alpha | ((srcColor >> 12) & 0x0f00) | ((srcColor >> 10) & 0x00f0) | ((srcColor >> 8) & 0x000f);

			*(u16*)dst = Common::swap16(val);
			dst+=2;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGBA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			RGBA_to_RGBA8(src, &dst[1], &dst[32], &dst[33], &dst[0]);
			src += readStride;
			dst += 2;
		}
		ENCODE_LOOP_SPANS2
		break;

	case GX_CTF_R4:
		SetBlockDimensions(3, 3, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		sBlkSize /= 2;
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			src += readStride;
			*dst = (srcColor >> 16) & 0xf0;

			srcColor = *(u32*)src;
			src += readStride;
			*dst |= (srcColor >> 20) & 0x0f;
			dst++;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RA4:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			src += readStride;
			*dst++ = ((srcColor << 2) & 0xf0) | ((srcColor >> 20) & 0x0f);
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			src += readStride;
			*dst++ = Convert6To8(srcColor & 0x3f);
			*dst++ = Convert6To8((srcColor >> 18) & 0x3f);
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_A8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			*dst++ = Convert6To8(srcColor & 0x3f);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_R8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			*dst++ = Convert6To8((srcColor >> 18) & 0x3f);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_G8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			*dst++ = Convert6To8((srcColor >> 12) & 0x3f);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_B8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			*dst++ = Convert6To8((srcColor >> 6) & 0x3f);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RG8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			src += readStride;
			*dst++ = Convert6To8((srcColor >> 12) & 0x3f);
			*dst++ = Convert6To8((srcColor >> 18) & 0x3f);
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_GB8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u32 srcColor = *(u32*)src;
			src += readStride;
			*dst++ = Convert6To8((srcColor >> 6) & 0x3f);
			*dst++ = Convert6To8((srcColor >> 12) & 0x3f);
		}
		ENCODE_LOOP_SPANS
		break;

	default:
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		break;
	}
}


static void EncodeRGBA6halfscale(u8 *dst, u8 *src, u32 format)
{
	u16 sBlkCount, tBlkCount, sBlkSize, tBlkSize;
	s32 tSpan, sBlkSpan, tBlkSpan, writeStride;
	u8 r, g, b, a;
	u32 readStride = 6;
	u8 *dstBlockStart = dst;

	switch (format)
	{
	case GX_TF_I4:
		SetBlockDimensions(3, 3, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		sBlkSize /= 2;
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_RGB8(src, &r, &g, &b);
			src += readStride;
			*dst = RGB8_to_I(r, g, b) & 0xf0;

			BoxfilterRGBA_to_RGB8(src, &r, &g, &b);
			src += readStride;
			*dst |= RGB8_to_I(r, g, b) >> 4;
			dst++;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_I8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_RGB8(src, &r, &g, &b);
			src += readStride;
			*dst++ = RGB8_to_I(r, g, b);
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_IA4:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_RGBA8(src, &r, &g, &b, &a);
			src += readStride;
			*dst++ = (a & 0xf0) | (RGB8_to_I(r, g, b) >> 4);
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_IA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_RGBA8(src, &r, &g, &b, &a);
			src += readStride;
			*dst++ = a;
			*dst++ = RGB8_to_I(r, g, b);
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGB565:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_RGB8(src, &r, &g, &b);
			src += readStride;

			u16 val = ((r << 8) & 0xf800) | ((g << 3) & 0x07e0) | ((b >> 3) & 0x001e);
			*(u16*)dst = Common::swap16(val);
			dst+=2;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGB5A3:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_RGBA8(src, &r, &g, &b, &a);
			src += readStride;

			u16 val;
			if (a >= 224) // 5551
				val = 0x8000 | ((r << 7) & 0x7c00) | ((g << 2) & 0x03e0) | ((b >> 3) & 0x001e);
			else // 4443
				val = ((a << 7) & 0x7000) | ((r << 4) & 0x0f00) | (g & 0x00f0) | ((b >> 4) & 0x000f);

			*(u16*)dst = Common::swap16(val);
			dst+=2;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGBA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_RGBA8(src, &dst[1], &dst[32], &dst[33], &dst[0]);
			src += readStride;
			dst += 2;
		}
		ENCODE_LOOP_SPANS2
		break;

	case GX_CTF_R4:
		SetBlockDimensions(3, 3, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		sBlkSize /= 2;
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_x8(src, &r, 18);
			src += readStride;
			*dst = r & 0xf0;

			BoxfilterRGBA_to_x8(src, &r, 18);
			src += readStride;
			*dst |= r >> 4;
			dst++;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RA4:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_xx8(src, &r, &a, 18, 0);
			src += readStride;
			*dst++ = (a & 0xf0) | (r >> 4);
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_xx8(src, &r, &a, 18, 0);
			src += readStride;
			*dst++ = a;
			*dst++ = r;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_A8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_x8(src, &a, 0);
			*dst++ = a;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_R8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_x8(src, &r, 18);
			*dst++ = r;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_G8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_x8(src, &g, 12);
			*dst++ = g;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_B8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_x8(src, &b, 6);
			*dst++ = b;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RG8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_xx8(src, &r, &g, 18, 12);
			src += readStride;
			*dst++ = g;
			*dst++ = r;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_GB8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGBA_to_xx8(src, &g, &b, 12, 6);
			src += readStride;
			*dst++ = b;
			*dst++ = g;
		}
		ENCODE_LOOP_SPANS
		break;

	default:
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		break;
	}
}

static void EncodeRGB8(u8 *dst, u8 *src, u32 format)
{
	u16 sBlkCount, tBlkCount, sBlkSize, tBlkSize;
	s32 tSpan, sBlkSpan, tBlkSpan, writeStride;
	u32 readStride = 3;
	u8 *dstBlockStart = dst;

	switch (format)
	{
	case GX_TF_I4:
		SetBlockDimensions(3, 3, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		sBlkSize /= 2;
		ENCODE_LOOP_BLOCKS
		{
			*dst = RGB8_to_I(src[2], src[1], src[0]) & 0xf0;
			src += readStride;

			*dst |= RGB8_to_I(src[2], src[1], src[0]) >> 4;
			src += readStride;
			dst++;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_I8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = RGB8_to_I(src[2], src[1], src[0]);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_IA4:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = 0xf0 | (RGB8_to_I(src[2], src[1], src[0]) >> 4);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_IA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = 0xff;
			*dst++ = RGB8_to_I(src[2], src[1], src[0]);

			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGB565:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u16 val = ((src[2] << 8) & 0xf800) | ((src[1] << 3) & 0x07e0) | ((src[0] >> 3) & 0x001e);
			*(u16*)dst = Common::swap16(val);
			src += readStride;
			dst+=2;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGB5A3:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			u16 val = 0x8000 | ((src[2] << 7) & 0x7c00) | ((src[1] << 2) & 0x03e0) | ((src[0] >> 3) & 0x001e);
			*(u16*)dst = Common::swap16(val);
			src += readStride;
			dst+=2;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGBA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			dst[0] = 0xff;
			dst[1] = src[2];
			dst[32] = src[1];
			dst[33] = src[0];
			src += readStride;
			dst += 2;
		}
		ENCODE_LOOP_SPANS2
		break;

	case GX_CTF_R4:
		SetBlockDimensions(3, 3, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		sBlkSize /= 2;
		ENCODE_LOOP_BLOCKS
		{
			*dst = src[2] & 0xf0;
			src += readStride;

			*dst |= src[2] >> 4;
			src += readStride;

			dst++;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RA4:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = 0xf0 | (src[2] >> 4);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = 0xff;
			*dst++ = src[2];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_A8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = 0xff;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_R8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = src[2];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_G8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = src[1];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_B8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = src[0];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RG8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = src[1];
			*dst++ = src[2];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_GB8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = src[0];
			*dst++ = src[1];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	default:
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		break;
	}
}

static void EncodeRGB8halfscale(u8 *dst, u8 *src, u32 format)
{
	u16 sBlkCount, tBlkCount, sBlkSize, tBlkSize;
	s32 tSpan, sBlkSpan, tBlkSpan, writeStride;
	u8 r, g, b;
	u32 readStride = 6;
	u8 *dstBlockStart = dst;

	switch (format)
	{
	case GX_TF_I4:
		SetBlockDimensions(3, 3, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		sBlkSize /= 2;
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_RGB8(src, &r, &g, &b);
			*dst = RGB8_to_I(r, g, b) & 0xf0;
			src += readStride;

			BoxfilterRGB_to_RGB8(src, &r, &g, &b);
			*dst |= RGB8_to_I(r, g, b) >> 4;
			src += readStride;
			dst++;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_I8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_RGB8(src, &r, &g, &b);
			*dst++ = RGB8_to_I(r, g, b);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_IA4:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_RGB8(src, &r, &g, &b);
			*dst++ = 0xf0 | (RGB8_to_I(r, g, b) >> 4);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_IA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_RGB8(src, &r, &g, &b);
			*dst++ = 0xff;
			*dst++ = RGB8_to_I(r, g, b);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGB565:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_RGB8(src, &r, &g, &b);
			u16 val = ((r << 8) & 0xf800) | ((g << 3) & 0x07e0) | ((b >> 3) & 0x001e);
			*(u16*)dst = Common::swap16(val);
			src += readStride;
			dst+=2;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGB5A3:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_RGB8(src, &r, &g, &b);
			u16 val = 0x8000 | ((r << 7) & 0x7c00) | ((g << 2) & 0x03e0) | ((b >> 3) & 0x001e);
			*(u16*)dst = Common::swap16(val);
			src += readStride;
			dst+=2;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_RGBA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_RGB8(src, &r, &g, &b);
			dst[0] = 0xff;
			dst[1] = r;
			dst[32] = g;
			dst[33] = b;
			src += readStride;
			dst += 2;
		}
		ENCODE_LOOP_SPANS2
		break;

	case GX_CTF_R4:
		SetBlockDimensions(3, 3, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		sBlkSize /= 2;
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_x8(src, &r, 2);
			*dst = r & 0xf0;
			src += readStride;

			BoxfilterRGB_to_x8(src, &r, 2);
			*dst |= r >> 4;
			src += readStride;

			dst++;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RA4:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_x8(src, &r, 2);
			*dst++ = 0xf0 | (r >> 4);
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RA8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_x8(src, &r, 2);
			*dst++ = 0xff;
			*dst++ = r;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_A8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = 0xff;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_R8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_x8(src, &r, 2);
			*dst++ = r;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_G8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_x8(src, &g, 1);
			*dst++ = g;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_B8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_x8(src, &b, 0);
			*dst++ = b;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_RG8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_xx8(src, &r, &g, 2, 1);
			*dst++ = g;
			*dst++ = r;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_GB8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_xx8(src, &g, &b, 1, 0);
			*dst++ = b;
			*dst++ = g;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	default:
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		break;
	}
}

static void EncodeZ24(u8 *dst, u8 *src, u32 format)
{
	u16 sBlkCount, tBlkCount, sBlkSize, tBlkSize;
	s32 tSpan, sBlkSpan, tBlkSpan, writeStride;
	u32 readStride = 3;
	u8 *dstBlockStart = dst;

	switch (format)
	{
	case GX_TF_Z8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = src[2];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_Z16:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = src[1];
			*dst++ = src[2];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_Z24X8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			dst[0] = 0xff;
			dst[1] = src[2];
			dst[32] = src[1];
			dst[33] = src[0];
			src += readStride;
			dst += 2;
		}
		ENCODE_LOOP_SPANS2
		break;

	case GX_CTF_Z4:
		SetBlockDimensions(3, 3, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		sBlkSize /= 2;
		ENCODE_LOOP_BLOCKS
		{
			*dst = src[2] & 0xf0;
			src += readStride;

			*dst |= src[2] >> 4;
			src += readStride;

			dst++;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_Z8M:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = src[1];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_Z8L:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = src[0];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_Z16L:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			*dst++ = src[0];
			*dst++ = src[1];
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	default:
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		break;
	}
}

static void EncodeZ24halfscale(u8 *dst, u8 *src, u32 format)
{
	u16 sBlkCount, tBlkCount, sBlkSize, tBlkSize;
	s32 tSpan, sBlkSpan, tBlkSpan, writeStride;
	u32 readStride = 6;
	u8 r, g, b;
	u8 *dstBlockStart = dst;

	switch (format)
	{
	case GX_TF_Z8:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_x8(src, &b, 2);
			*dst++ = b;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_Z16:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_xx8(src, &g, &b, 1, 2);
			*dst++ = b;
			*dst++ = g;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_TF_Z24X8:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_RGB8(src, &dst[33], &dst[32], &dst[1]);
			dst[0] = 255;
			src += readStride;
			dst += 2;
		}
		ENCODE_LOOP_SPANS2
		break;

	case GX_CTF_Z4:
		SetBlockDimensions(3, 3, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		sBlkSize /= 2;
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_x8(src, &b, 2);
			*dst = b & 0xf0;
			src += readStride;

			BoxfilterRGB_to_x8(src, &b, 2);
			*dst |= b >> 4;
			src += readStride;

			dst++;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_Z8M:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_x8(src, &g, 1);
			*dst++ = g;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_Z8L:
		SetBlockDimensions(3, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_x8(src, &r, 0);
			*dst++ = r;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	case GX_CTF_Z16L:
		SetBlockDimensions(2, 2, &sBlkCount, &tBlkCount, &sBlkSize, &tBlkSize);
		SetSpans(sBlkSize, tBlkSize, &tSpan, &sBlkSpan, &tBlkSpan, &writeStride);
		ENCODE_LOOP_BLOCKS
		{
			BoxfilterRGB_to_xx8(src, &r, &g, 0, 1);
			*dst++ = g;
			*dst++ = r;
			src += readStride;
		}
		ENCODE_LOOP_SPANS
		break;

	default:
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		break;
	}
}

void Encode(u8 *dest_ptr)
{
	auto pixelformat = bpmem.zcontrol.pixel_format;
	bool bFromZBuffer = pixelformat == PEControl::Z24;
	bool bIsIntensityFmt = bpmem.triggerEFBCopy.intensity_fmt > 0;
	u32 copyfmt = ((bpmem.triggerEFBCopy.target_pixel_format / 2) + ((bpmem.triggerEFBCopy.target_pixel_format & 1) * 8));

	// pack copy format information into a single variable
	u32 format = copyfmt;
	if (bFromZBuffer)
	{
		format |= _GX_TF_ZTF;
		if (copyfmt == 11)
			format = GX_TF_Z16;
		else if (format < GX_TF_Z8 || format > GX_TF_Z24X8)
			format |= _GX_TF_CTF;
	}
	else
		if (copyfmt > GX_TF_RGBA8 || (copyfmt < GX_TF_RGB565 && !bIsIntensityFmt))
			format |= _GX_TF_CTF;

	u8 *src = EfbInterface::GetPixelPointer(bpmem.copyTexSrcXY.x, bpmem.copyTexSrcXY.y, bFromZBuffer);

	if (bpmem.triggerEFBCopy.half_scale)
	{
		if (pixelformat == PEControl::RGBA6_Z24)
			EncodeRGBA6halfscale(dest_ptr, src, format);
		else if (pixelformat == PEControl::RGB8_Z24)
			EncodeRGB8halfscale(dest_ptr, src, format);
		else if (pixelformat == PEControl::RGB565_Z16)  // not supported
			EncodeRGB8halfscale(dest_ptr, src, format);
		else if (pixelformat == PEControl::Z24)
			EncodeZ24halfscale(dest_ptr, src, format);
	}
	else
	{
		if (pixelformat == PEControl::RGBA6_Z24)
			EncodeRGBA6(dest_ptr, src, format);
		else if (pixelformat == PEControl::RGB8_Z24)
			EncodeRGB8(dest_ptr, src, format);
		else if (pixelformat == PEControl::RGB565_Z16)  // not supported
			EncodeRGB8(dest_ptr, src, format);
		else if (pixelformat == PEControl::Z24)
			EncodeZ24(dest_ptr, src, format);
	}
}


}
