// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "VideoCommon/TexelDecoder.h"
#include "VideoCommon/TextureDecoderFunctions.h"

namespace TexelDecoder
{

void DecodeTexel(u8 *dst, const u8 *src, int s, int t, int imageWidth, int texformat, int tlutaddr, int tlutfmt)
{
	/* General formula for computing texture offset
	//
	u16 sBlk = s / blockWidth;
	u16 tBlk = t / blockHeight;
	u16 widthBlks = (width / blockWidth) + 1;
	u32 base = (tBlk * widthBlks + sBlk) * blockWidth * blockHeight;
	u16 blkS = s & (blockWidth - 1);
	u16 blkT =  t & (blockHeight - 1);
	u32 blkOff = blkT * blockWidth + blkS;
	*/

	switch (texformat)
	{
	case GX_TF_C4:
		{
			u16 sBlk = s >> 3;
			u16 tBlk = t >> 3;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5;
			u16 blkS = s & 7;
			u16 blkT =  t & 7;
			u32 blkOff = (blkT << 3) + blkS;

			int rs = (blkOff & 1)?0:4;
			u32 offset = base + (blkOff >> 1);

			u8 val = (*(src + offset) >> rs) & 0xF;
			u16 *tlut = (u16*)(texMem + tlutaddr);

			switch (tlutfmt)
			{
			case 0:
				*((u32*)dst) = decodeIA8Swapped(tlut[val]);
				break;
			case 1:
				*((u32*)dst) = decode565RGBA(Common::swap16(tlut[val]));
				break;
			case 2:
				*((u32*)dst) = decode5A3RGBA(Common::swap16(tlut[val]));
				break;
			}
		}
		break;
	case GX_TF_I4:
		{
			u16 sBlk = s >> 3;
			u16 tBlk = t >> 3;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5;
			u16 blkS = s & 7;
			u16 blkT =  t & 7;
			u32 blkOff = (blkT << 3) + blkS;

			int rs = (blkOff & 1)?0:4;
			u32 offset = base + (blkOff >> 1);

			u8 val = (*(src + offset) >> rs) & 0xF;
			val = Convert4To8(val);
			dst[0] = val;
			dst[1] = val;
			dst[2] = val;
			dst[3] = val;
		}
	   break;
	case GX_TF_I8:
		{
			u16 sBlk = s >> 3;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5;
			u16 blkS = s & 7;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 3) + blkS;

			u8 val = *(src + base + blkOff);
			dst[0] = val;
			dst[1] = val;
			dst[2] = val;
			dst[3] = val;
		}
		break;
	case GX_TF_C8:
		{
			u16 sBlk = s >> 3;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5;
			u16 blkS = s & 7;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 3) + blkS;

			u8 val = *(src + base + blkOff);
			u16 *tlut = (u16*)(texMem + tlutaddr);

			switch (tlutfmt)
			{
			case 0:
				*((u32*)dst) = decodeIA8Swapped(tlut[val]);
				break;
			case 1:
				*((u32*)dst) = decode565RGBA(Common::swap16(tlut[val]));
				break;
			case 2:
				*((u32*)dst) = decode5A3RGBA(Common::swap16(tlut[val]));
				break;
			}
		}
		break;
	case GX_TF_IA4:
		{
			u16 sBlk = s >> 3;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5;
			u16 blkS = s & 7;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 3) + blkS;

			u8 val = *(src + base + blkOff);
			const u8 a = Convert4To8(val>>4);
			const u8 l = Convert4To8(val&0xF);
			dst[0] = l;
			dst[1] = l;
			dst[2] = l;
			dst[3] = a;
		}
		break;
	case GX_TF_IA8:
		{
			u16 sBlk = s >> 2;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 2) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 4;
			u16 blkS = s & 3;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 2) + blkS;

			u32 offset = (base + blkOff) << 1;
			const u16* valAddr = (u16*)(src + offset);

			*((u32*)dst) = decodeIA8Swapped(*valAddr);
		}
		break;
	case GX_TF_C14X2:
		{
			u16 sBlk = s >> 2;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 2) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 4;
			u16 blkS = s & 3;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 2) + blkS;

			u32 offset = (base + blkOff) << 1;
			const u16* valAddr = (u16*)(src + offset);

			u16 val = Common::swap16(*valAddr) & 0x3FFF;
			u16 *tlut = (u16*)(texMem + tlutaddr);

			switch (tlutfmt)
			{
			case 0:
				*((u32*)dst) = decodeIA8Swapped(tlut[val]);
				break;
			case 1:
				*((u32*)dst) = decode565RGBA(Common::swap16(tlut[val]));
				break;
			case 2:
				*((u32*)dst) = decode5A3RGBA(Common::swap16(tlut[val]));
				break;
			}
		}
		break;
	case GX_TF_RGB565:
		{
			u16 sBlk = s >> 2;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 2) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 4;
			u16 blkS = s & 3;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 2) + blkS;

			u32 offset = (base + blkOff) << 1;
			const u16* valAddr = (u16*)(src + offset);

			*((u32*)dst) = decode565RGBA(Common::swap16(*valAddr));
		}
		break;
	case GX_TF_RGB5A3:
		{
			u16 sBlk = s >> 2;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 2) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 4;
			u16 blkS = s & 3;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 2) + blkS;

			u32 offset = (base + blkOff) << 1;
			const u16* valAddr = (u16*)(src + offset);

			*((u32*)dst) = decode5A3RGBA(Common::swap16(*valAddr));
		}
		break;
	case GX_TF_RGBA8:
		{
			u16 sBlk = s >> 2;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 2) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5; // shift by 5 is correct
			u16 blkS = s & 3;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 2) + blkS;

			u32 offset = (base + blkOff) << 1 ;
			const u8* valAddr = src + offset;

			dst[3] = valAddr[0];
			dst[0] = valAddr[1];
			dst[1] = valAddr[32];
			dst[2] = valAddr[33];
		}
		break;
	case GX_TF_CMPR:
		{
			u16 sDxt = s >> 2;
			u16 tDxt = t >> 2;

			u16 sBlk = sDxt >> 1;
			u16 tBlk = tDxt >> 1;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 2;
			u16 blkS = sDxt & 1;
			u16 blkT =  tDxt & 1;
			u32 blkOff = (blkT << 1) + blkS;

			u32 offset = (base + blkOff) << 3;

			const DXTBlock* dxtBlock = (const DXTBlock*)(src + offset);

			u16 c1 = Common::swap16(dxtBlock->color1);
			u16 c2 = Common::swap16(dxtBlock->color2);
			int blue1 = Convert5To8(c1 & 0x1F);
			int blue2 = Convert5To8(c2 & 0x1F);
			int green1 = Convert6To8((c1 >> 5) & 0x3F);
			int green2 = Convert6To8((c2 >> 5) & 0x3F);
			int red1 = Convert5To8((c1 >> 11) & 0x1F);
			int red2 = Convert5To8((c2 >> 11) & 0x1F);

			u16 ss = s & 3;
			u16 tt = t & 3;

			int colorSel = dxtBlock->lines[tt];
			int rs = 6 - (ss << 1);
			colorSel = (colorSel >> rs) & 3;
			colorSel |= c1 > c2?0:4;

			u32 color = 0;

			switch (colorSel)
			{
				case 0:
				case 4:
					color = makeRGBA(red1, green1, blue1, 255);
					break;
				case 1:
				case 5:
					color = makeRGBA(red2, green2, blue2, 255);
					break;
				case 2:
					color = makeRGBA(red1+(red2-red1)/3, green1+(green2-green1)/3, blue1+(blue2-blue1)/3, 255);
					break;
				case 3:
					color = makeRGBA(red2+(red1-red2)/3, green2+(green1-green2)/3, blue2+(blue1-blue2)/3, 255);
					break;
				case 6:
					color = makeRGBA((int)ceil((float)(red1+red2)/2), (int)ceil((float)(green1+green2)/2), (int)ceil((float)(blue1+blue2)/2), 255);
					break;
				case 7:
					color = makeRGBA(red2, green2, blue2, 0);
					break;
			}

			*((u32*)dst) = color;
		}
		break;
	}
}

void DecodeTexelRGBA8FromTmem(u8 *dst, const u8 *src_ar, const u8* src_gb, int s, int t, int imageWidth)
{
	u16 sBlk = s >> 2;
	u16 tBlk = t >> 2;
	u16 widthBlks = (imageWidth >> 2) + 1; // TODO: Looks wrong. Shouldn't this be ((imageWidth-1)>>2)+1 ?
	u32 base_ar = (tBlk * widthBlks + sBlk) << 4;
	u32 base_gb = (tBlk * widthBlks + sBlk) << 4;
	u16 blkS = s & 3;
	u16 blkT =  t & 3;
	u32 blk_off = (blkT << 2) + blkS;

	u32 offset_ar = (base_ar + blk_off) << 1;
	u32 offset_gb = (base_gb + blk_off) << 1;
	const u8* val_addr_ar = src_ar + offset_ar;
	const u8* val_addr_gb = src_gb + offset_gb;

	dst[3] = val_addr_ar[0]; // A
	dst[0] = val_addr_ar[1]; // R
	dst[1] = val_addr_gb[0]; // G
	dst[2] = val_addr_gb[1]; // B
}

PC_TexFormat DecodeRGBA8FromTmem(u8* dst, const u8 *src_ar, const u8 *src_gb, int width, int height)
{
	// TODO for someone who cares: Make this less slow!
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			DecodeTexelRGBA8FromTmem(dst, src_ar, src_gb, x, y, width-1);
			dst += 4;
		}
	return PC_TEX_FMT_RGBA32;
}


}
