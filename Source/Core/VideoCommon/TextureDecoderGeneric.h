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
	void Decode_C4(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	void Decode_C8(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	void Decode_I4(u32 *dst, const u8 *src, int width, int height);
	void Decode_I8(u32 *dst, const u8 *src, int width, int height);
	void Decode_IA4(u32 *dst, const u8 *src, int width, int height);
	void Decode_IA8(u32 *dst, const u8 *src, int width, int height);
	void Decode_C14X2(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	void Decode_RGB565(u32 *dst, const u8 *src, int width, int height);
	void Decode_RGB5A3(u32 *dst, const u8 *src, int width, int height);
	void Decode_RGBA8(u32 *dst, const u8 *src, int width, int height);
	void Decode_CMPR(u32 *dst, const u8 *src, int width, int height);
};
