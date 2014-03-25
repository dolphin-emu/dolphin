// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// Texel Decoding
// Mostly only used in software rasterizer
// Is used when decoding RGBA8 textures from Tmem as well
namespace TexelDecoder
{
	void DecodeTexel(u8 *dst, const u8 *src, int s, int t, int imageWidth, int texformat, int tlutaddr, int tlutfmt);
	void DecodeTexelRGBA8FromTmem(u8 *dst, const u8 *src_ar, const u8* src_gb, int s, int t, int imageWidth);
	void DecodeRGBA8FromTmem(u8* dst, const u8 *src_ar, const u8 *src_gb, int width, int height);
}
