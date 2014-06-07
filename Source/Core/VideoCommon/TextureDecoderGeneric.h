// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/TextureDecoder.h"

class TextureDecoderGeneric : public TextureDecoder
{
	private:
	// Private texture decoders
	// Regular CPU texture decoding
	PC_TexFormat Decode_C4(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	PC_TexFormat Decode_C8(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	PC_TexFormat Decode_I4(u32 *dst, const u8 *src, int width, int height);
	PC_TexFormat Decode_I8(u32 *dst, const u8 *src, int width, int height);
	PC_TexFormat Decode_IA4(u32 *dst, const u8 *src, int width, int height);
	PC_TexFormat Decode_IA8(u32 *dst, const u8 *src, int width, int height);
	PC_TexFormat Decode_C14X2(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	PC_TexFormat Decode_RGB565(u32 *dst, const u8 *src, int width, int height);
	PC_TexFormat Decode_RGB5A3(u32 *dst, const u8 *src, int width, int height);
	PC_TexFormat Decode_RGBA8(u32 *dst, const u8 *src, int width, int height);
	PC_TexFormat Decode_CMPR(u32 *dst, const u8 *src, int width, int height);
};
