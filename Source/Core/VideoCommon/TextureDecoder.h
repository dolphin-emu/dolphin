// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/Hash.h"
#include "VideoCommon/TextureDecoderHelpers.h"

enum
{
	TMEM_SIZE = 1024*1024,
	TMEM_LINE_SIZE = 32,
};
extern  GC_ALIGNED16(u8 texMem[TMEM_SIZE]);

class TextureDecoder
{
	private:
	bool m_TexFmt_Overlay_Enable;
	bool m_TexFmt_Overlay_Center;

	// Private texture decoders
	virtual void Decode_C4(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt) = 0;
	virtual void Decode_C8(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt) = 0;
	virtual void Decode_I4(u32 *dst, const u8 *src, int width, int height) = 0;
	virtual void Decode_I8(u32 *dst, const u8 *src, int width, int height) = 0;
	virtual void Decode_IA4(u32 *dst, const u8 *src, int width, int height) = 0;
	virtual void Decode_IA8(u32 *dst, const u8 *src, int width, int height) = 0;
	virtual void Decode_C14X2(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt) = 0;
	virtual void Decode_RGB565(u32 *dst, const u8 *src, int width, int height) = 0;
	virtual void Decode_RGB5A3(u32 *dst, const u8 *src, int width, int height) = 0;
	virtual void Decode_RGBA8(u32 *dst, const u8 *src, int width, int height) = 0;
	virtual void Decode_CMPR(u32 *dst, const u8 *src, int width, int height) = 0;

	public:
	virtual ~TextureDecoder() {}
	TextureDecoder() : m_TexFmt_Overlay_Enable(false), m_TexFmt_Overlay_Center(false) {}

	// Texture format overlay option setting
	virtual void SetTextureFormatOverlay(bool enable, bool center) final;

	// Calls our internal texture decoders
	// Also puts the texture overlay upon the final texture image
	virtual void Decode(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt) final;
};

std::unique_ptr<TextureDecoder> CreateTextureDecoder();
