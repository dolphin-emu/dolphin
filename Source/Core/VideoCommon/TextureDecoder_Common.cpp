// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include "Common/Common.h"

#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/sfont.inc"
#include "VideoCommon/TextureDecoder.h"

static bool TexFmt_Overlay_Enable=false;
static bool TexFmt_Overlay_Center=false;

// TRAM
// STATE_TO_SAVE
GC_ALIGNED16(u8 texMem[TMEM_SIZE]);

int TexDecoder_GetTexelSizeInNibbles(int format)
{
	switch (format & 0x3f) {
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
	case GX_CTF_R4:    return 1;
	case GX_CTF_RA4:   return 2;
	case GX_CTF_RA8:   return 4;
	case GX_CTF_YUVA8: return 8;
	case GX_CTF_A8:    return 2;
	case GX_CTF_R8:    return 2;
	case GX_CTF_G8:    return 2;
	case GX_CTF_B8:    return 2;
	case GX_CTF_RG8:   return 4;
	case GX_CTF_GB8:   return 4;

	case GX_TF_Z8:     return 2;
	case GX_TF_Z16:    return 4;
	case GX_TF_Z24X8:  return 8;

	case GX_CTF_Z4:    return 1;
	case GX_CTF_Z8M:   return 2;
	case GX_CTF_Z8L:   return 2;
	case GX_CTF_Z16L:  return 4;
	default: return 1;
	}
}

int TexDecoder_GetTextureSizeInBytes(int width, int height, int format)
{
	return (width * height * TexDecoder_GetTexelSizeInNibbles(format)) / 2;
}

int TexDecoder_GetBlockWidthInTexels(u32 format)
{
	switch (format)
	{
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
	case GX_CTF_R4: return 8;
	case GX_CTF_RA4: return 8;
	case GX_CTF_RA8: return 4;
	case GX_CTF_A8: return 8;
	case GX_CTF_R8: return 8;
	case GX_CTF_G8: return 8;
	case GX_CTF_B8: return 8;
	case GX_CTF_RG8: return 4;
	case GX_CTF_GB8: return 4;
	case GX_TF_Z8: return 8;
	case GX_TF_Z16: return 4;
	case GX_TF_Z24X8: return 4;
	case GX_CTF_Z4: return 8;
	case GX_CTF_Z8M: return 8;
	case GX_CTF_Z8L: return 8;
	case GX_CTF_Z16L: return 4;
	default:
		ERROR_LOG(VIDEO, "Unsupported Texture Format (%08x)! (GetBlockWidthInTexels)", format);
		return 8;
	}
}

int TexDecoder_GetBlockHeightInTexels(u32 format)
{
	switch (format)
	{
	case GX_TF_I4: return 8;
	case GX_TF_I8: return 4;
	case GX_TF_IA4: return 4;
	case GX_TF_IA8: return 4;
	case GX_TF_RGB565: return 4;
	case GX_TF_RGB5A3: return 4;
	case GX_TF_RGBA8: return  4;
	case GX_TF_C4: return 8;
	case GX_TF_C8: return 4;
	case GX_TF_C14X2: return 4;
	case GX_TF_CMPR: return 8;
	case GX_CTF_R4: return 8;
	case GX_CTF_RA4: return 4;
	case GX_CTF_RA8: return 4;
	case GX_CTF_A8: return 4;
	case GX_CTF_R8: return 4;
	case GX_CTF_G8: return 4;
	case GX_CTF_B8: return 4;
	case GX_CTF_RG8: return 4;
	case GX_CTF_GB8: return 4;
	case GX_TF_Z8: return 4;
	case GX_TF_Z16: return 4;
	case GX_TF_Z24X8: return 4;
	case GX_CTF_Z4: return 8;
	case GX_CTF_Z8M: return 4;
	case GX_CTF_Z8L: return 4;
	case GX_CTF_Z16L: return 4;
	default:
		ERROR_LOG(VIDEO, "Unsupported Texture Format (%08x)! (GetBlockHeightInTexels)", format);
		return 4;
	}
}

//returns bytes
int TexDecoder_GetPaletteSize(int format)
{
	switch (format)
	{
	case GX_TF_C4: return 16 * 2;
	case GX_TF_C8: return 256 * 2;
	case GX_TF_C14X2: return 16384 * 2;
	default:
		return 0;
	}
}

void TexDecoder_SetTexFmtOverlayOptions(bool enable, bool center)
{
	TexFmt_Overlay_Enable = enable;
	TexFmt_Overlay_Center = center;
}

static const char* texfmt[] = {
	// pixel
	"I4",      "I8",      "IA4",      "IA8",
	"RGB565",  "RGB5A3",  "RGBA8",    "0x07",
	"C4",      "C8",      "C14X2",    "0x0B",
	"0x0C",    "0x0D",    "CMPR",     "0x0F",
	// Z-buffer
	"0x10",    "Z8",      "0x12",     "Z16",
	"0x14",    "0x15",    "Z24X8",    "0x17",
	"0x18",    "0x19",    "0x1A",     "0x1B",
	"0x1C",    "0x1D",    "0x1E",     "0x1F",
	// pixel + copy
	"CR4",     "0x21",    "CRA4",    "CRA8",
	"0x24",    "0x25",    "CYUVA8",  "CA8",
	"CR8",     "CG8",     "CB8",     "CRG8",
	"CGB8",    "0x2D",    "0x2E",    "0x2F",
	// Z + copy
	"CZ4",     "0x31",    "0x32",    "0x33",
	"0x34",    "0x35",    "0x36",    "0x37",
	"0x38",    "CZ8M",    "CZ8L",    "0x3B",
	"CZ16L",   "0x3D",    "0x3E",    "0x3F",
};

static void TexDecoder_DrawOverlay(u8 *dst, int width, int height, int texformat, PC_TexFormat pc_texformat)
{
	int w = std::min(width, 40);
	int h = std::min(height, 10);

	int xoff = (width - w) >> 1;
	int yoff = (height - h) >> 1;

	if (!TexFmt_Overlay_Center)
	{
		xoff=0;
		yoff=0;
	}

	const char* fmt = texfmt[texformat&15];
	while (*fmt)
	{
		int xcnt = 0;
		int nchar = sfont_map[(int)*fmt];

		const unsigned char *ptr = sfont_raw[nchar]; // each char is up to 9x10

		for (int x = 0; x < 9;x++)
		{
			if (ptr[x] == 0x78)
				break;
			xcnt++;
		}

		for (int y=0; y < 10; y++)
		{
			for (int x=0; x < xcnt; x++)
			{
				switch (pc_texformat)
				{
				case PC_TEX_FMT_I8:
				{
					// TODO: Is this an acceptable way to draw in I8?
					u8  *dtp = (u8*)dst;
					dtp[(y + yoff) * width + x + xoff] = ptr[x] ? 0xFF : 0x88;
					break;
				}
				case PC_TEX_FMT_IA8:
				case PC_TEX_FMT_IA4_AS_IA8:
				{
					u16  *dtp = (u16*)dst;
					dtp[(y + yoff) * width + x + xoff] = ptr[x] ? 0xFFFF : 0xFF00;
					break;
				}
				case PC_TEX_FMT_RGB565:
				{
					u16  *dtp = (u16*)dst;
					dtp[(y + yoff)*width + x + xoff] = ptr[x] ? 0xFFFF : 0x0000;
					break;
				}
				default:
				case PC_TEX_FMT_BGRA32:
				{
					int  *dtp = (int*)dst;
					dtp[(y + yoff) * width + x + xoff] = ptr[x] ? 0xFFFFFFFF : 0xFF000000;
					break;
				}
				}
			}
			ptr += 9;
		}
		xoff += xcnt;
		fmt++;
	}
}

PC_TexFormat TexDecoder_Decode(u8 *dst, const u8 *src, int width, int height, int texformat, const u8* tlut, TlutFormat tlutfmt)
{
	PC_TexFormat pc_texformat = _TexDecoder_DecodeImpl((u32*)dst, src, width, height, texformat, tlut, tlutfmt);

	if (TexFmt_Overlay_Enable && pc_texformat != PC_TEX_FMT_NONE)
		TexDecoder_DrawOverlay(dst, width, height, texformat, pc_texformat);

	return pc_texformat;
}

static inline u32 DecodePixel_IA8(u16 val)
{
	int a = val & 0xFF;
	int i = val >> 8;
	return i | (i<<8) | (i<<16) | (a<<24);
}

static inline u32 DecodePixel_RGB565(u16 val)
{
	int r,g,b,a;
	r=Convert5To8((val>>11) & 0x1f);
	g=Convert6To8((val>>5 ) & 0x3f);
	b=Convert5To8((val    ) & 0x1f);
	a=0xFF;
	return  r | (g<<8) | (b << 16) | (a << 24);
}

static inline u32 DecodePixel_RGB5A3(u16 val)
{
	int r,g,b,a;
	if ((val&0x8000))
	{
		r=Convert5To8((val>>10) & 0x1f);
		g=Convert5To8((val>>5 ) & 0x1f);
		b=Convert5To8((val    ) & 0x1f);
		a=0xFF;
	}
	else
	{
		a=Convert3To8((val>>12) & 0x7);
		r=Convert4To8((val>>8 ) & 0xf);
		g=Convert4To8((val>>4 ) & 0xf);
		b=Convert4To8((val    ) & 0xf);
	}
	return r | (g<<8) | (b << 16) | (a << 24);
}

static inline u32 DecodePixel_Paletted(u16 pixel, TlutFormat tlutfmt)
{
	switch (tlutfmt)
	{
	case GX_TL_IA8:
		return DecodePixel_IA8(pixel);
	case GX_TL_RGB565:
		return DecodePixel_RGB565(Common::swap16(pixel));
	case GX_TL_RGB5A3:
		return DecodePixel_RGB5A3(Common::swap16(pixel));
	default:
		return 0;
	}
}

struct DXTBlock
{
	u16 color1;
	u16 color2;
	u8 lines[4];
};

static inline u32 MakeRGBA(int r, int g, int b, int a)
{
	return (a<<24)|(b<<16)|(g<<8)|r;
}

void TexDecoder_DecodeTexel(u8 *dst, const u8 *src, int s, int t, int imageWidth, int texformat, const u8* tlut_, TlutFormat tlutfmt)
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
			u16 *tlut = (u16*) tlut_;

			*((u32*)dst) = DecodePixel_Paletted(tlut[val], tlutfmt);
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
			u16 *tlut = (u16*) tlut_;

			*((u32*)dst) = DecodePixel_Paletted(tlut[val], tlutfmt);
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

			*((u32*)dst) = DecodePixel_IA8(*valAddr);
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
			u16 *tlut = (u16*) tlut_;

			*((u32*)dst) = DecodePixel_Paletted(tlut[val], tlutfmt);
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

			*((u32*)dst) = DecodePixel_RGB565(Common::swap16(*valAddr));
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

			*((u32*)dst) = DecodePixel_RGB5A3(Common::swap16(*valAddr));
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
					color = MakeRGBA(red1, green1, blue1, 255);
					break;
				case 1:
				case 5:
					color = MakeRGBA(red2, green2, blue2, 255);
					break;
				case 2:
					color = MakeRGBA(red1+(red2-red1)/3, green1+(green2-green1)/3, blue1+(blue2-blue1)/3, 255);
					break;
				case 3:
					color = MakeRGBA(red2+(red1-red2)/3, green2+(green1-green2)/3, blue2+(blue1-blue2)/3, 255);
					break;
				case 6:
					color = MakeRGBA((int)ceil((float)(red1+red2)/2), (int)ceil((float)(green1+green2)/2), (int)ceil((float)(blue1+blue2)/2), 255);
					break;
				case 7:
					color = MakeRGBA(red2, green2, blue2, 0);
					break;
			}

			*((u32*)dst) = color;
		}
		break;
	}
}

void TexDecoder_DecodeTexelRGBA8FromTmem(u8 *dst, const u8 *src_ar, const u8* src_gb, int s, int t, int imageWidth)
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

PC_TexFormat TexDecoder_DecodeRGBA8FromTmem(u8* dst, const u8 *src_ar, const u8 *src_gb, int width, int height)
{
	// TODO for someone who cares: Make this less slow!
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			TexDecoder_DecodeTexelRGBA8FromTmem(dst, src_ar, src_gb, x, y, width-1);
			dst += 4;
		}

	return PC_TEX_FMT_RGBA32;
}
