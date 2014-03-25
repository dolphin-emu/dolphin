// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "VideoCommon/TextureDecoderHelpers.h"

namespace TextureDecoderTools
{
	// Helper functions
	int GetTexelSizeInNibbles(int format)
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

	int GetTextureSizeInBytes(int width, int height, int format)
	{
		return (width * height * GetTexelSizeInNibbles(format)) / 2;
	}

	int GetBlockWidthInTexels(u32 format)
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

	int GetBlockHeightInTexels(u32 format)
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
	int GetPaletteSize(int format)
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

	PC_TexFormat GetPCFormatFromTLUTFormat(int tlutfmt)
	{
		switch (tlutfmt)
		{
		case 0: return PC_TEX_FMT_IA8;    // IA8
		case 1: return PC_TEX_FMT_RGB565; // RGB565
		case 2: return PC_TEX_FMT_BGRA32; // RGB5A3: This TLUT format requires
										  // extra work to decode.
		}
		return PC_TEX_FMT_NONE; // Error
	}

	PC_TexFormat GetPCTexFormat(int texformat, int tlutfmt)
	{
		switch (texformat)
		{
		case GX_TF_C4:
			return GetPCFormatFromTLUTFormat(tlutfmt);
		case GX_TF_I4:
			return PC_TEX_FMT_IA8;
		case GX_TF_I8:  // speed critical
			return PC_TEX_FMT_IA8;
		case GX_TF_C8:
			return GetPCFormatFromTLUTFormat(tlutfmt);
		case GX_TF_IA4:
			return PC_TEX_FMT_IA4_AS_IA8;
		case GX_TF_IA8:
			return PC_TEX_FMT_IA8;
		case GX_TF_C14X2:
			return GetPCFormatFromTLUTFormat(tlutfmt);
		case GX_TF_RGB565:
			return PC_TEX_FMT_RGB565;
		case GX_TF_RGB5A3:
			return PC_TEX_FMT_BGRA32;
		case GX_TF_RGBA8:  // speed critical
			return PC_TEX_FMT_BGRA32;
		case GX_TF_CMPR:  // speed critical
			// The metroid games use this format almost exclusively.
			{
				return PC_TEX_FMT_BGRA32;
			}
		}

		// The "copy" texture formats, too?
		return PC_TEX_FMT_NONE;
	}
}
